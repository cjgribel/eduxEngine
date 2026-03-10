# Task Visualizer Plan

This note outlines a low-intrusion design for a job graph / task visualizer that shows:

- frame stages
- thread-pool work
- serialized strand work
- main-thread queue work
- queued time vs run time
- queue stalls and blocking waits

The goal is not just performance analysis, but also human understanding of what the engine is actually doing in parallel.

## Why This Fits This Codebase

The current engine already routes most asynchronous work through a few central execution points:

- generic worker execution in [ThreadPool.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/util/ThreadPool.hpp)
- serialized "strand" execution in [SerialExecutor.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/util/SerialExecutor.hpp)
- main-thread handoff in [MainThreadQueue.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/MainThreadQueue.hpp)
- batch orchestration on a strand in [BatchRegistry.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/BatchRegistry.hpp#L241) and [BatchRegistry.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/BatchRegistry.cpp#L1839)
- existing coarse queue stats in [EngineOverlayGui.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/EngineOverlayGui.cpp#L279)
- an editor `Task Monitor` window already exists in [GuiManager.cpp](/Users/ag1498/GitHub/eduxEngine/src/editor/gui/GuiManager.cpp#L960)

That makes this a good candidate for centralized instrumentation rather than invasive, subsystem-by-subsystem tracing.

## Problem Statement

The existing profiler in [Profiler.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/util/Profiler.hpp) is useful for aggregate timings, but it does not answer concurrency questions well:

- what actually ran in parallel?
- what was queued but not running?
- what blocked on the main thread?
- which strand is the bottleneck?
- how long did a task wait before starting?

Those questions require a trace/timeline model, not just aggregated category totals.

## Design Goals

- Make parallel execution visible in a way humans can follow.
- Avoid invasive instrumentation of all systems.
- Keep runtime overhead small and controllable.
- Work with thread pools and strand queues, not just worker threads.
- Fit naturally into the editor and existing debug UI.

## Non-Goals For V1

- Full lock-free distributed tracing across every subsystem
- Automatic causal graph reconstruction for every lambda
- Perfect production-grade profiling precision
- GPU tracing

V1 should be practical and useful, not academically complete.

## Core Idea

Instrument executor boundaries, not the work itself.

That means the first implementation should emit events when tasks move through these stages:

- enqueued
- dequeued
- started
- finished
- wait-begin
- wait-end

If we do that in the central queue/executor types, we get visibility over a large part of the engine without changing every async call site.

## Recommended Architecture

### 1. Add a `TaskTrace` service

Create a runtime-owned tracing service that stores task execution events.

Suggested responsibilities:

- allocate task ids
- record lightweight events
- maintain executor/lane metadata
- provide snapshots to editor UI
- optionally prune/roll windows of trace history

This should live beside existing engine services, not inside `ThreadPool` or `Profiler`.

Potential home:

- `EngineServices`
- or a new lightweight service shared through `EngineContext`

The engine already constructs thread pool and main-thread queue centrally in [EngineContext.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/EngineContext.cpp#L72), which is a good place to wire the trace service in.

### 2. Use event tracing, not only timer accumulation

Suggested event structure:

```cpp
enum class TaskTraceEventType : uint8_t {
    Enqueue,
    Start,
    Finish,
    WaitBegin,
    WaitEnd
};

enum class TaskExecutorKind : uint8_t {
    ThreadPool,
    SerialExecutor,
    MainThreadQueue,
    EventQueue,
    Unknown
};

struct TaskTraceEvent {
    uint64_t timestamp_us = 0;
    uint64_t task_id = 0;
    uint64_t parent_task_id = 0;
    uint32_t thread_id = 0;
    uint32_t lane_id = 0;
    TaskTraceEventType type = TaskTraceEventType::Enqueue;
    TaskExecutorKind executor_kind = TaskExecutorKind::Unknown;
    uint32_t queue_depth = 0;
    const char* label = nullptr;
};
```

The most important fields for V1 are:

- timestamp
- task id
- lane id
- event type
- thread id
- optional label
- queue depth

`parent_task_id` is valuable, but can be added after the basic timeline works.

### 3. Add named lanes

The visualization should not only show raw worker threads. It should also show logical lanes:

- `ThreadPool Worker 0..N`
- `Main Thread Queue`
- `BatchRegistry Strand`
- `ResourceManager Strand`
- `Event Queue` if instrumented later

This is especially important because strand queues are often where humans lose the plot: lots of work is "async" but still serialized.

## Instrumentation Points

### 1. ThreadPool

The best first hook is [ThreadPool.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/util/ThreadPool.hpp#L58) and [ThreadPool.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/util/ThreadPool.cpp).

At enqueue time:

- allocate task id
- record `Enqueue`
- snapshot queue depth if cheap

At worker start:

- record `Start`

At worker finish:

- record `Finish`

This alone gives real visibility into parallelism and queue pressure.

### 2. SerialExecutor

[SerialExecutor.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/util/SerialExecutor.hpp#L31) is the next most important place.

This executor already exposes:

- `running()`
- `queued()`
- `is_busy()`

That is excellent for UI summaries. Add trace emission to:

- `post`
- beginning of `drain` task execution
- end of task execution

Each `SerialExecutor` instance should have a stable lane id and a friendly name.

Suggested names:

- `BatchRegistry`
- `ResourceManager`
- `UnknownStrandN`

### 3. MainThreadQueue

[MainThreadQueue.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/MainThreadQueue.hpp#L18) is where cross-thread handoff becomes especially interesting.

Instrument:

- `push` as `Enqueue`
- main-thread execution in `execute_all` as `Start` and `Finish`
- `push_and_wait` with explicit `WaitBegin` and `WaitEnd`

This is likely the most valuable stall signal in the whole engine, because it exposes when workers are blocked waiting for the main thread.

### 4. BatchRegistry

Batch work is already serialized through a strand in [BatchRegistry.cpp](/Users/ag1498/GitHub/eduxEngine/src/engine/BatchRegistry.cpp#L1839).

This makes BatchRegistry a perfect first "named strand" for visualization. It should show:

- queue depth over time
- task labels like `Load Batch`, `Unload Batch`, `Create Entity`, `Attach Entity`
- time spent waiting vs executing

### 5. ResourceManager

The concrete implementation is not shown here, but [IResourceManager.hpp](/Users/ag1498/GitHub/eduxEngine/src/engine/engineapi/IResourceManager.hpp#L81) already exposes:

- `is_busy()`
- `queued_tasks()`

That strongly suggests an internal queue or serialized work model already exists. Once the concrete `ResourceManager` executor path is instrumented, it should become another named lane in the visualizer.

## Task Labels

This is the main interpretation problem.

Many current tasks are anonymous lambdas, so pure executor instrumentation will initially show timing and overlap but not always meaningful names.

Recommended approach:

### V1

- labels optional
- unnamed tasks still visible
- fallback label may be executor-specific, such as `ThreadPool Task` or `BatchRegistry Task`

### V2

Introduce small helpers such as:

```cpp
pool.queue_task(trace::named("Import assets", [=] { ... }));
strand.submit(trace::named("Batch load", [=] { ... }));
main_thread_queue->push(trace::named("Bind refs", [=] { ... }));
```

This allows progressive adoption without forcing every call site to change immediately.

### V3

Add thread-local "current task id" so tasks scheduled from within tasks can inherit a parent link, enabling a lightweight causal graph.

## UI Design

The natural home for this is the existing `Task Monitor` window in [GuiManager.cpp](/Users/ag1498/GitHub/eduxEngine/src/editor/gui/GuiManager.cpp#L960).

### V1 UI

- current thread pool stats
- current main-thread queue stats
- strand queue stats
- recent task table:
  - task label
  - executor
  - queued time
  - run time
  - total lifetime
- simple queue-depth plots for thread pool and important strands

### V2 UI

- swimlane timeline over the last N frames or M milliseconds
- one row per worker thread
- one row per named strand
- colored blocks for running tasks
- gap markers for queued-but-not-running delay
- stall markers for `push_and_wait`

### V3 UI

- parent/child task expansion
- filter by subsystem
- filter by task label
- show only tasks touching the main thread

## Overhead Strategy

Your concern about intrusiveness is valid.

This system should be designed to fail safe:

- off by default in release builds
- enabled in debug/editor builds
- bounded memory via ring buffers
- avoid locks on every event if possible
- use per-thread buffers or batched flushes where practical

The wrong design is:

- locking a global vector on every enqueue/start/finish

The better design is:

- thread-local small buffers
- periodic merge on main thread
- or an MPSC ring buffer if needed later

V1 can start simpler if event rate is low enough, then be optimized only if it proves necessary.

## Relationship To The Existing Profiler

Keep the current profiler.

It solves a different problem:

- `Profiler`: category totals, averages, frame phase cost
- `TaskTrace`: overlap, queue delays, stalls, execution order

The two systems complement each other well. For example:

- `Profiler` says `Resource load` averaged 8 ms
- `TaskTrace` says 5 ms of that was queue wait on a strand and only 3 ms was actual execution

That kind of answer is exactly why this visualizer is valuable.

## Recommended MVP

Build the first version in this order:

1. Add `TaskTrace` service and a small event model.
2. Instrument `ThreadPool`.
3. Instrument `MainThreadQueue`.
4. Instrument `SerialExecutor`.
5. Give BatchRegistry strand a stable display name.
6. Extend the existing `Task Monitor` window with:
   - queue stats
   - recent task table
   - queued vs run time
7. Add optional task labels to a few high-value paths:
   - batch load/unload
   - resource load/bind
   - asset import
   - main-thread bind/ref tasks

This already delivers a lot of value and avoids UI overreach.

## Good Signals To Surface

The most useful early metrics are:

- average queue wait time per executor
- worst queue wait time
- number of tasks that blocked on main thread
- longest-running worker task
- strand backlog over time
- percentage of frame where all workers were idle but queues were non-empty

That last one is especially good at revealing scheduling bugs or hidden serialization.

## Risks

### 1. Event volume

If too many tiny tasks are traced, the UI can become noisy and the collector expensive.

Mitigation:

- bounded trace windows
- min-duration filters in the UI
- optional sampling

### 2. Unlabeled tasks

A timeline full of unnamed lambdas is still technically correct, but less useful.

Mitigation:

- progressive labeling helpers
- named strands
- subsystem-specific executor names

### 3. Trace overhead changing behavior

This is the core concern.

Mitigation:

- keep the first version at executor boundaries only
- avoid deep call-site instrumentation
- keep tracing togglable

## Design Recommendation

This feature is worth pursuing, and it is less invasive than it first appears.

For this codebase, the best strategy is:

- do not try to build a full job graph first
- start with executor-lane tracing
- make queue wait visible
- make `push_and_wait` visible
- attach names only where it adds clarity

If done that way, this becomes a very practical observability tool rather than a risky rewrite.

## Likely Follow-Up Notes

Once this starts moving, useful follow-up notes would be:

- `TaskTraceEventSchema.md`
- `TaskMonitorWidgetPlan.md`
- `ResourceManagerStrandInstrumentation.md`
- `BatchTaskLabels.md`
