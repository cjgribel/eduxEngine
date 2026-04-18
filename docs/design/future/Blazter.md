# Simple Platformer plan

Sandboxy, Phyics-based, Run-and-Gun type of game where you run (or fly) around capturing
monsters or annoying each other.
The core is the fun of playing around with a jet-pack and a vaccum/gun inspired by Slime Rancher, and Luigi's Mansion.
Should be multiplayer, either via one shared view [Fallguys] or split-screen (affects engine/pipeline).

## Simple animated characters

- Basic graph with animation for running, jumping, flying etc.
- Force based jet pack with particles

## Bidirectional vacuum/gun

- Vacuum-mode for capturing monsters, and possible annoying other players
- - Particles? How do I make particles go "backward"?
- Shoot-mode with particles for stunning, annoying, pushing etc
- Maybe a lock-on mode for monsters

## Monsters

- AI? Follow-paths? Escape?
- Different types and abilities (fast, shoots something ...)

## Gameplay

Collect monsters and drop them off somewhere. Either one-by-one, or via some
kind of backpack storage.

## Multiplayer

- Same view. Fallguys style.
- Split screen

## New tech

- PlayerController ECS with anim graph
- Enemy ECS with anim graph
- Gun ECS
- Capture logic
- Terrain mesh asset
- 