

#if 0

	t2Body* bodyparts[16];
	int bodypart_nbrVecs[16] = { 5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
	vec2f bodypart_pos[16];
	vec2f body_vecs[] = {
		vec2f(-0.0933f, 0.0632f),
		vec2f(-0.1033f, -0.0468f),
		vec2f(0.1407f, -0.0448f),
		vec2f(0.1387f, -0.0148f),
		vec2f(-0.0093f, 0.0632f)
	};

	vec_it = 0;
	for(int i = 0; i < 16; i++)
	{
		bodyparts[i] = new t2Body(pos + bodypart_pos[i], false);
		bodyparts[i]->setMass(mass_scale * t2d_poly_get);
		bodyparts[i]->collisionGroupId = collisionGroupId;
		bodyparts[i]->collisionFilter = COLLISION_GROUP_ALL ^ collisionGroupId;

		t2PolygonGeometry *pgeom = new t2PolygonGeometry();
		for(int j = 0; j < bodypart_nbrVecs[i], j++)
		{
			pgeom->addVertex(body_vecs[vec_it + j]);
		}
		vec_it += bodypart_nbrVecs[i];

		bodyparts[i]->addGeometry(pgeom);
		world->addBody(bodyparts[i]);
	}

	vec2f vecs[] = { vec2f(-0.0933f, 0.0632f), vec2f(-0.1033f, -0.0468f), vec2f(0.1407f, -0.0448f),
		vec2f(0.1387f, -0.0148f), vec2f(-0.0093f, 0.0632f) };
	vec2f vecs[] = { vec2f(0.0f, 0.0f), vec2f(4.0f, 0.0f), vec2f(4.0f, 4.0f), vec2f(0.0f, 4.0f), };
	float I_test = t2d_poly_I(vecs, 4, 1.0f);
	printf("poly_I %f\n", I_test);

	 bodies, vecs, nbr_vecs, pos
	int test[] = {1, 1};
	int test3[2] = { test };
	int test2[][2] = { {1, 1}, {1, 1} };



	
	 distance joint rendered as a multi-stroke hydraulic cylinder
	
	class t2dHydraulicActuator : public t2DistanceJoint
	{
	public:
	
		t2dHydraulicActuator(t2Body* bodyA, t2Body* bodyB, vec2f anchorA, vec2f anchorB,
			float L, float stroke_width, float stroke_height)
			: t2DistanceJoint(bodyA, bodyB, anchorA, anchorB, L),
			stroke_width(stroke_width), stroke_height(stroke_height), stroke_dwidth(-0.15f)
		{ }
	
		void render()
		{
			vec2f
				vA = bodyA->X + mat2(bodyA->R) * anchorA,			// global anchor point for A
				vB = bodyB->X + mat2(bodyB->R) * anchorB,			// global anchor point for B
				vAB = vB - vA;										// joint vector
			float
				vAB_len = vAB.norm();								// 
			vec2f
				vABn = vAB /vAB_len,								// normalized joint vector
				v = vA,												// initial point along joint vector
				v_inc = vABn * stroke_height,						// increment per stroke along joint vector
				vn = vec2f::getNormal(vA, vB).normalize() * (stroke_width/2.0f),	// joint vector normal
				vn_inc = vn * stroke_dwidth;						// increment of normal width (stroke width) per stroke
			int
				nbrFullStrokes = vAB_len / stroke_height;			// 
			
			glColor4f(0.3f, 0.3f, 0.3f, 1.0f);
			for(int i = 0; i < nbrFullStrokes; i++)					// render; a square per full stroke
			{
				vn += vn_inc;
				glBegin(GL_LINE_LOOP);
				glVertex2d(v.x - vn.x, v.y - vn.y);
				glVertex2d(v.x + vn.x, v.y + vn.y);
				v += v_inc;
				glVertex2d(v.x + vn.x, v.y + vn.y);
				glVertex2d(v.x - vn.x, v.y - vn.y);
				glEnd();
			}
			vn += vn_inc;											// render the final, partial stroke
			glBegin(GL_LINE_LOOP);
			glVertex2d(v.x - vn.x, v.y - vn.y);
			glVertex2d(v.x + vn.x, v.y + vn.y);
			v = vB;
			glVertex2d(v.x + vn.x, v.y + vn.y);
			glVertex2d(v.x - vn.x, v.y - vn.y);
			glEnd();
		}
	
	private:
		float stroke_width, stroke_dwidth, stroke_height;
	};



	inline t2dContactPoint FindCP(vec2f v1, vec2f v2, t2PolygonGeometry *pgeom)
	{
		vec2f
			cp = v1,					// clip point
			ev, en,						// edge vector / normal
			ivec = v2 - v1;				// intersecting vector
		float
			frac_out, frac_in;			// fraction of intersecting vector being outside / inside geometry
		t2dContactPoint result;
	
		for(int i = 0; i < pgeom->nbrVertices; i++)
		{
			ev = pgeom->vertices[i];
			en = pgeom->normals[i];
			if(!VertexInsideEdge(cp, ev, en))
			{
				frac_out = fabs(vec2f::dot(cp - ev, en));
				frac_in = fabs(vec2f::dot(v2 - ev, en));
				cp += ivec * (frac_out / (frac_out + frac_in));
				ivec = v2 - cp;
	
				result.cp = cp;
				result.cn = en;
				result.ei = i;
			}
		}
		return result;
	
	}



	 check for overlapping pairs
	
	// sweep x
	for(int i = 0; i < nbrElements; i++)
	{
		elem = list_x[i];
		if(elem.type == ElementType::MIN)
		{
			active.push_back(elem);
		}
		else
		{
			active.remove(elem);
			for(std::list<ListElement>::iterator it = active.begin(); it != active.end(); it++)
			{
				if(elem.body != it->body)
					candidates_tmp.push_back(t2ElementPair(elem, *it));
			}
		}
	}
	// sweep y
	active.clear();
	for(int i = 0; i < nbrElements; i++)
	{
		elem = list_y[i];
		if(elem.type == ElementType::MIN)
		{
			active.push_back(elem);
		}
		else
		{
			active.remove(elem);
			for(std::list<ListElement>::iterator it = active.begin(); it != active.end(); it++)
			{
				if(elem.body != it->body && candidateExist(elem, *it))
					candidates.push_back(t2ElementPair(elem, *it));
			}
		}
	}



	inline void insertionSort(ListElement list[]){
		ListElement elem;
		int j;
		for(int i = 1; i < nbrElements; i++)
		{
			elem = list[i];
			j = i - 1;
			while(j >= 0 && *list[j].value > *elem.value)
			{
				list[j+1] = list[j];
				j--;
			}
			list[j+1] = elem;
		}
	}

	bool candidateExist(ListElement elemA, ListElement elemB)
	{
		t2ElementPair elemPair(elemA, elemB);
		for(std::list<t2ElementPair>::iterator it = candidates_tmp.begin(); it != candidates_tmp.end(); it++)
		{
			if(elemPair == *it)
				return true;
		}
		return false;
	}



	inline void swapAdjacency_x(ListElement elemA, ListElement elemB)
	{
		if(elemA.body == elemB.body || elemA.type == elemB.type)
			return;

		if(elemA.type == ElementType::MIN && elemB.type == ElementType::MAX)
		{
			candidates.remove(t2ElementPair(elemA, elemB));
		}
		else if(elemA.type == ElementType::MAX && elemB.type == ElementType::MIN)
		{
			if(elemA.geom->aabb.y_min < elemB.geom->aabb.y_max && elemA.geom->aabb.y_max > elemB.geom->aabb.y_min)
				candidates.push_back(t2ElementPair(elemA, elemB));
		}



	for(int i = 1; i < nbrElements; i++)
	{
		j = i - 1;
		if(j >= 0 && *list_x[j].value > *list_x[i].value)
		{
			elem = list_x[i];
			removeAdjacency(list_x[j], list_x[i]); /*printf("rem0 %i, %i\n", j, i);*/
			if((i+1) < nbrElements) { removeAdjacency(list_x[i], list_x[i+1]); /*printf("rem1 %i, %i\n", i, i+1);*/ }

			while(j >= 0 && *list_x[j].value > *elem.value)
			{
				list_x[j+1] = list_x[j];
				if((j-1) >= 0) { removeAdjacency(list_x[j-1], list_x[j]); /*printf("rem2 %i, %i\n", j-1, j);*/ }
				if((j+2) < nbrElements) { addAdjacency(list_x[j], list_x[j+2]); /*printf("add3 %i, %i\n", j, j+2);*/ }
				j--;
			}

			list_x[j+1] = elem;
			if(j >= 0) { addAdjacency(list_x[j], elem); /*printf("add4 %i, elem\n", j);*/ }
			addAdjacency(elem, list_x[j+2]); /*printf("add5 elem, %i\n", j+1);*/
		}
	}



	class t2dPoly : public t2Body
	{
	public:
	
		//vec2f vertices[15];			// always rel COM
		//int nbrVertices;				// 
	
		//vec2f vertices_tfm[15];		// updates every step
		//vec2f vertices_tfm_prev[15];	// 
		//vec2f normals[15];				// normals
		//vec2f normals_prev[15];		// 
	
		//std::list<vec2f> pocs, pocs_rej;	// pocs = points of contact, pocs_rej = pocs with vRel > 0
		//vec2f marker;
		//vec2f dX;
		
		t2dPoly(vec2f pos, vec2f size, bool isStatic)
			: t2Body(pos, 1.0f, isStatic)
		{
			nbrVertices = 4;
			vec2f ds = size * 0.5f;
			vertices[0].set(ds.x, -ds.y);	// lower-right
			vertices[1].set(ds.x, ds.y);	// upper-right
			vertices[2].set(-ds.x, ds.y);	// upper-left
			vertices[3].set(-ds.x, -ds.y);	// lower-left
			//vertices[4].set(0, -2.f*ds.y);
			
			// n-sided poly
			//vec2f ds = size * .5f;
			//nbrVertices = 8;
			//float drad = 360.f / nbrVertices;
			//for(int i = 0; i < nbrVertices; i++) {
			//	vec2f v = mat2((float)i * drad) * vec2f(1.f, 1.f);
			//	vertices[i].set(v.x * ds.x, v.y * ds.y);
			//}
		}
	};

	 *** inertia tensor at z-axis for rectangle disc
	 at COM: Iz = m(x^2+y^2)/12
	 at an axis vector d from COM: Iz + m|d|^2

	 inertia tensor for rectangle: I = m/12*(w^2 + h^2) + m*d^2
	 where h = height, w = width, d = distance from center of mass
	float Irect(t2Body* const body, vec2f d);


	 static friction
	float maxFriction_s = 0.4f * lambdaAcc_n;
	float maxFriction_d = bodyB->friction * lambdaAcc_n;
	if(fabs(lambda_t + lambdaAcc_t) < 0.4f * fabs(lambdaAcc_n))
		lambda_t = clampf(lambdaAcc_t + lambda_t, -maxFriction_s, maxFriction_s) - lambdaAcc_t;
	else
		lambda_t = clampf(lambdaAcc_t + lambda_t, -maxFriction_d, maxFriction_d) - lambdaAcc_t;
	if(fabs(lambda_t) < fabs(lambdaAcc_t))
		lambdaAcc_t += lambda_t;
	else
		lambdaAcc_t = 0.0f;



	// spring force
	vec2f l = mat2(bodyA->R) * vec2f(1.0f, 0.0f); 
	vec2f lu = vec2f::cross(l, 1.0f);	// l x z = -y

	rA = mat2(bodyA->R) * anchorA;
	rB = mat2(bodyB->R) * anchorB;
	rAB = bodyA->X + rA - bodyB->X - rB;

	vec2f rABu = vec2f::projection(rAB, lu);
	vec2f rABun = rABu * (1.0f / rABu.norm());
	springForce = (rABu - rABun * L) * K;

	// damper force
	rAdot = bodyA->V + vec2f::cross(bodyA->W * DEG_TO_RAD, rA);
	rBdot = bodyB->V + vec2f::cross(bodyB->W * DEG_TO_RAD, rB);
	damperForce = rABun * vec2f::dot(rAdot - rBdot, rABun) * D;

	// apply to bodies
	sumForce = springForce + damperForce;
	bodyA->applyForce(-sumForce, rA);
	bodyB->applyForce(sumForce, rB);



	 addPyramid
	world->addBody(new t2Box(
		vec2f(-((float)(levels-hi-1)*dim/2.0f) - spacing*(float)(levels-hi)/2.0f + wi*(dim+spacing), y + dim*(0.5f+hi) + (hi+1)*spacing),
		dim, dim, 1.0f, false));



	
	void collisionDetect4(){
		t2dPoly *bodyA, *bodyB;
		//std::list<t2dPoly*> correctedBodies, correctedBodiesTmp;
		int cds;

		for(int cdit = 0; cdit < 3; cdit++){
			cds = 0;

			// perform one pass of CD
			for(int i = 0; i < nbrBodies; i++){
				bodyA = bodies[i];	
				bodyA->pocs.clear();
				for(int j = i+1; j < nbrBodies; j++){
					bodyB = bodies[j];

					// poor man's broad phase CD: assume max size of objects and test if they're close enough
					if((bodyA->isStatic || bodyA->isStatic) || vec2f::getNorm(bodyA->X - bodyB->X) < 1.5f) {

						bool bodyApenetB = isInsideBody(bodyA, bodyB);
						bool bodyBpenetA = isInsideBody(bodyB, bodyA);

						// decide if bodies are colliding
						if(bodyApenetB || bodyBpenetA){
							if(bodyApenetB){
								float toc = manageCollision(bodyA, bodyB); // float toci = toc; // 1.0f-toc;
								vec2f VAtmp = bodyA->V, VBtmp = bodyB->V; float WAtmp = bodyA->W, WBtmp = bodyB->W;
								vec2f VAt = bodyA->Vprev + (bodyA->Vorg - bodyA->Vprev) * toc + (bodyA->V-bodyA->Vorg);
								float WAt = bodyA->Wprev + (bodyA->Worg - bodyA->Wprev) * toc + (bodyA->W-bodyA->Worg);
								vec2f VBt = bodyB->Vprev + (bodyB->Vorg - bodyB->Vprev) * toc + (bodyB->V-bodyB->Vorg);
								float WBt = bodyB->Wprev + (bodyB->Worg - bodyB->Wprev) * toc + (bodyB->W-bodyB->Worg);
								bodyA->V = VAt; bodyA->W = WAt;	bodyB->V = VBt; bodyB->W = WBt;
								manageCorrection(bodyA, bodyB, 1.0f);
								bodyA->V = VAtmp+(bodyA->V-VAt); bodyA->W = WAtmp+(bodyA->W-WAt); bodyB->V = VBtmp+(bodyB->V-VBt); bodyB->W = WBtmp+(bodyB->W-WBt);
							}
							if(bodyBpenetA){
								float toc = manageCollision(bodyB, bodyA); // float toci = toc; // 1.0f-toc;
								vec2f VAtmp = bodyA->V, VBtmp = bodyB->V; float WAtmp = bodyA->W, WBtmp = bodyB->W;
								vec2f VAt = bodyA->Vprev + (bodyA->Vorg - bodyA->Vprev) * toc + (bodyA->V-bodyA->Vorg);
								float WAt = bodyA->Wprev + (bodyA->Worg - bodyA->Wprev) * toc + (bodyA->W-bodyA->Worg);
								vec2f VBt = bodyB->Vprev + (bodyB->Vorg - bodyB->Vprev) * toc + (bodyB->V-bodyB->Vorg);
								float WBt = bodyB->Wprev + (bodyB->Worg - bodyB->Wprev) * toc + (bodyB->W-bodyB->Worg);
								bodyA->V = VAt; bodyA->W = WAt;	bodyB->V = VBt; bodyB->W = WBt;
								manageCorrection(bodyB, bodyA, 1.0f);
								bodyA->V = VAtmp+(bodyA->V-VAt); bodyA->W = WAtmp+(bodyA->W-WAt); bodyB->V = VBtmp+(bodyB->V-VBt); bodyB->W = WBtmp+(bodyB->W-WBt);
							}
							cds++;
						} else {
							// no collision
						}
					} // if close enough

				} // for j
			} // for i

			//printf("it %i, cds %i\n", cdit, cds);
		} // it

	}

	 manage collision A -> B; calculate response, correct positions
	float manageCollision(t2dPoly* bodyA, t2dPoly* bodyB){
	
		std::list<PocData> pocsData;	// collsion data; pocs, nocs, penetrations
		float min_ratio = 1.0f;

		// iterate vertices of A, collect collision data
		//printf("*** new cd ***\n");
		for(int k = 0; k < bodyA->nbrVertices; k++){
			if(isInsideBody(bodyA->vertices_tfm[k], bodyB)) {
				// vertex is penetrating B

				// set up vectors
				vec2f vAfrom = bodyA->vertices_tfm_prev[k];	// 
				vec2f vAto = bodyA->vertices_tfm[k];			// 
				vec2f vA = vAto - vAfrom;						// (rel) linear motion of A since last dt

				vec2f vBfrom = bodyB->vertices_tfm_prev[0];	//
				vec2f vBto = bodyB->vertices_tfm[0];			//
				vec2f vB = vBto - vBfrom;						// (rel) linear motion of B since last dt

				vec2f vAB = vA - vB;							// (rel) combined motion of A & B
				vec2f vABfrom = vAfrom;
				vec2f vABto = vAfrom + vAB;
				PocData pocData;

				// decide pseudo-poc
				vec2f vEdgePoint, vEdgeNormal;					// point/normal from edge in B
				vec2f vABtmp = vAB;							// 
				vec2f pseudoPoc = vAfrom;
				for(int l = 0; l < bodyB->nbrVertices; l++){	// iteration needed?
					vEdgePoint = bodyB->vertices_tfm_prev[l];
					vEdgeNormal = bodyB->normals_prev[l];
					if(!isInsideEdge(pseudoPoc, vEdgePoint, vEdgeNormal)){
						float dotOut = fabs(vec2f::dot(pseudoPoc - vEdgePoint, vEdgeNormal));
						float dotIn = fabs(vec2f::dot(vABto - vEdgePoint, vEdgeNormal));
						pseudoPoc += vABtmp * (dotOut / (dotOut + dotIn));
						vABtmp = vABto - pseudoPoc;
						
						//pocData.poc = poc;
						pocData.noc = bodyB->normals[l]; // vEdgeNormal; <- prev normal
					}
				}

				float ratio = vec2f::getNorm(pseudoPoc - vABfrom) / vAB.norm();
				min_ratio = std::min(ratio, min_ratio);
				//printf("ratio: %f\n", ratio);
				pocData.poc = vAfrom + (vA * ratio);
				pocData.penetA = vA * (1.0f - ratio);
				pocData.penetB = vB * (1.0f - ratio);
				pocsData.push_back(pocData);
			} // if isInsideBody
		} // for k

		return min_ratio;

		// yield responses, calculate max penetrations vectors
		if(pocsData.size() > 0){	// if 0, no collision after all
			vec2f maxApenet, maxBpenet;
			float maxAdepth = 0, maxBdepth = 0;

			for(std::list<PocData>::iterator it = pocsData.begin(); it != pocsData.end(); it++){
				bodyA->pocs.push_back(it->poc);
				//yieldResponse(bodyA, bodyB, it->poc, it->noc);
				if(it->penetA.norm() > maxAdepth) {
					maxApenet = it->penetA;
					maxAdepth = it->penetA.norm();
				}
				if(it->penetB.norm() > maxBdepth) {
					maxBpenet = it->penetB;
					maxBdepth = it->penetB.norm();
				}
			}

			// correct body positions
			// - really no need to check for static bodies unless they move
			if(!bodyA->isStatic)
				correctBody(bodyA, -maxApenet);
			if(!bodyB->isStatic)
				correctBody(bodyB, -maxBpenet);

		} // if(pocsData.size() > 0)	
	} // manageCollision

	
	 force correct A out of B
	
	void manageCorrection(t2dPoly* bodyA, t2dPoly* bodyB, float toc){

		std::list<PocResult> edges;		// intersected edges of B
		std::list<vec2f> vertices;		// penetrating vertices
		vec2f vThis, vNext;			//
		bool thisInside, nextInside;	//

		// iterate vertices of A, collect penetrating vertices & intersected edges
		for(int i = 0; i < bodyA->nbrVertices; i++){
			vThis = bodyA->vertices_tfm[i]; vNext = bodyA->vertices_tfm[(i + 1) % bodyA->nbrVertices];
			thisInside = isInsideBody(vThis, bodyB); nextInside = isInsideBody(vNext, bodyB);
			// store penetrating vertex
			if(thisInside)
				vertices.push_back(vThis);
			// store edge if intersected
			if(thisInside && !nextInside){
				edges.push_back(findPoc(vThis, vNext, bodyB));
			} else if(!thisInside && nextInside){
				edges.push_back(findPoc(vNext, vThis, bodyB));
			}
		}
		//printf("%i, %i\n", vertices.size(), edges.size());

		vec2f vFinCorr;						// final correction vector
		float vFinCorrl = INFINITE;				// 
		vec2f mpoc, mn, mpv, poc, n, pv;		// poc, n, pvec (for response) (max, temp)
		std::list<float> mdepths, depths;		// penetration depths for vertices (final, temp)
		float mtotdepth = 0, totdepth = 0;		// total depth of all vertices
		
		// find shortest correcting vector with respect to each intersected edge
		if(edges.size() > 0) {
			for(std::list<PocResult>::iterator eit = edges.begin(); eit != edges.end(); eit++){
				vec2f maxCorr;				// max correcting vector for this edge
				float maxCorrl = 0;			// 
				depths.clear();				// 
				totdepth = 0;				// 

				for(std::list<vec2f>::iterator vit = vertices.begin(); vit != vertices.end(); vit++){
					vec2f vCorr = vec2f::projection(*vit - eit->poc, -eit->normal);	// correcting vector fot his vertex
					float vCorrl = vCorr.norm();										//
					if(vCorrl > maxCorrl){
						maxCorr = vCorr;
						maxCorrl = vCorrl;
						poc = eit->poc; n = eit->normal; pv = *vit;
					}
					depths.push_back(vCorrl);
					totdepth += vCorrl;
				}
				// decide if this correcting vector is currently the shortest among edges
				maxCorrl = maxCorr.norm();
				if(maxCorrl < vFinCorrl){
					vFinCorr = maxCorr;
					vFinCorrl = maxCorrl;
					mpoc = poc; mn = n; mpv = pv;
					mdepths = depths; mtotdepth = totdepth;
				}
			}
			//printf("FinCorrl: %f\n", vFinCorrl);

			// yield response
			// use mpv-vFinCorr?
			yieldResponse(bodyA, bodyB, mpv-vFinCorr, mn, vertices, mdepths, mtotdepth, toc);

			// execute correction
			correctPair(bodyA, bodyB, -vFinCorr, mpv, mpv-vFinCorr);

		} // if(edges.size() > 0)

	} //forceCorrect
	
	 vCorr points toward body A
	void correctPair(t2dPoly* bodyA, t2dPoly* bodyB, vec2f vCorr, vec2f rA, vec2f rB){

		vec2f vCorrA, vCorrB;

		// decide how the correcting vector should be split between the bodies
		// later: weight with graph mass

		if(!bodyA->isStatic && !bodyB->isStatic){
			// neither body is static, weight correction by masses
			float f = 1.f / (bodyA->mass + bodyB->mass);
			vCorrA = vCorr * (1.f - bodyA->mass * f);
			vCorrB = -vCorr + vCorrA;
			//correctedBodies.push_back(bodyA);
			//correctedBodies.push_back(bodyB);
		} else if(bodyA->isStatic && !bodyB->isStatic){
			// body A is static, correct only B
			vCorrB = -vCorr;
			//correctedBodies.push_back(bodyB);
		} else if (!bodyA->isStatic && bodyB->isStatic){
			// body B is static, correct only A
			vCorrA = vCorr;
			//correctedBodies.push_back(bodyA);
		} else {
			// both bodies are static, undefined behaviour. split in half.
			/*vCorrA = vCorr * .5f;
			vCorrB = -vCorrA;*/
		}

		//float pJ = vCorr.normSquared()

		// correct the graph of A
		//std::list<t2dPoly*> visited;
		//visited.push_back(bodyB);	// in case of cyclic dependence
		correctBodyRec(bodyA, vCorrA, rA);

		// correct the graph of B
		//visited.clear();
		//visited.push_back(bodyA);	// in case of cyclic dependence
		correctBodyRec(bodyB, vCorrB, rB);
	}

	 
	void correctBodyRec(t2dPoly* body, vec2f vCorr, vec2f r){

		if(!body->isStatic){
			// time 1, density 1
			//float k = 5.0f, d = 1.0f;
			//vec2f J = vCorr * (vCorr.normSquared()*k + -vec2f::dot(vCorr, body->V)*d);
			//vec2f J;
			//float l = vCorr.norm();
			//J = vCorr * 0.05f; // std::max(0.0f, (l-0.01f)/l) * 0.1f;

			//body->X += J * body->imass;
			//body->R += vec2f::cross(r - body->X, J) * 1.0f/Irect(body, r - body->X) * RAD_TO_DEG;

			float ERP = 0.8f; // say ERP = 2*1/iterations
			//float ERP = .3f; // say ERP = 2*1/iterations
			correctBody(body, vCorr * ERP);

		}
	}	
	
	 execute correction of body
	void correctBody(t2dPoly* body, vec2f vCorr){
		body->X += vCorr;
		for(int i = 0; i < body->nbrVertices; i++)
			body->vertices_tfm[i] += vCorr;
		//glBegin(GL_LINES); glVertex2d(body->X.x, body->X.y); glVertex2d(body->X.x + vCorr.x, body->X.y + vCorr.y); glEnd();
	}

	 is any vertex of A inside B
	/*bool isInsideBody(t2dPoly* bodyA, t2dPoly* bodyB){
		bool colliding = false;
		for(int i = 0; i < bodyA->nbrVertices; i++)
			colliding |= isInsideBody(bodyA->vertices_tfm[i], bodyB);
		return colliding;
	}*/

	
	 is any vertex of A inside B - at previous dt
	/*bool isInsideBody_prev(t2dPoly* bodyA, t2dPoly* bodyB){
		bool colliding = false;
		for(int i = 0; i < bodyA->nbrVertices; i++)
			colliding |= isInsideBody_prev(bodyA->vertices_tfm_prev[i], bodyB);
		return colliding;
	}*/
	
	 is vertex inside body
	bool isInsideBody(vec2f &vertex, t2dPoly *body){
		bool inside = true;
		for(int i = 0; i < body->nbrVertices; i++)
			inside &= isInsideEdge(vertex, body->vertices_tfm[i], body->normals[i]);
		return inside;
	}

	
	 is vertex inside body - at previous dt
	/*bool isInsideBody_prev(vec2f &vertex, t2dPoly *body){
		bool inside = true;
		for(int i = 0; i < body->nbrVertices; i++)
			inside &= isInsideEdge(vertex, body->vertices_tfm_prev[i], body->normals_prev[i]);
		return inside;
	}*/

	
	 is vertex inside edge
	/*bool isInsideEdge(vec2f &vertex, vec2f &edge_vertex, vec2f &edge_normal){
		return vec2f::dot(vertex - edge_vertex, edge_normal) < 0;
	}*/

	// find intersection point (poc) and normal between vector (vec_to-vec_from) and body
	// assume vec_to is inside body
	PocResult findPoc(vec2f vec_to, vec2f vec_from, t2dPoly *body) { 
		vec2f edge_vec, n;
		vec2f poc = vec_from;
		vec2f vec = vec_to - vec_from;
		PocResult res;

		for(int i = 0; i < body->nbrVertices; i++){
			edge_vec = body->vertices_tfm[i];
			n = body->normals[i];
			if(!isInsideEdge(poc, edge_vec, n)){
				float dot_out = fabs(vec2f::dot(poc - edge_vec, n));
				float dot_in = fabs(vec2f::dot(vec_to - edge_vec, n));
				poc += vec * (dot_out / (dot_out + dot_in));
				vec = vec_to - poc;

				res.normal = n;
				res.poc = poc;
			}
		}
		return res;
	}


	
	
	bool yieldResponse(t2dPoly *bodyA, t2dPoly *bodyB, vec2f &poc, vec2f &n,
		std::list<vec2f> pocs, std::list<float> depths, float totdepth, float toc) { 

		float C = bodyA->cor * bodyB->cor;

		vec2f rA = poc - bodyA->X;
		vec2f rB = poc - bodyB->X;

		vec2f rAdot = bodyA->V + vec2f(-bodyA->W * DEG_TO_RAD * rA.y, bodyA->W * DEG_TO_RAD * rA.x);	// (eqn a1)
		vec2f rBdot = bodyB->V + vec2f(-bodyB->W * DEG_TO_RAD * rB.y, bodyB->W * DEG_TO_RAD * rB.x);	// (eqn a1)

		vec2f drdot = rAdot - rBdot;
		float vRelPre = vec2f::dot(n, drdot);					// (eqn 23)

		// continue only if bodies move toward each other
		/*if(vRelPre < 0 && vRelPre > -.2f){
			return true;
		} else*/
			if(vRelPre < 0.0f){

			float c1 = vec2f::cross(rA, n) * 1.0f/Irect(bodyA, rA); // bodyA->iI;
			vec2f c1b = vec2f(-c1 * rA.y, c1 * rA.x);			// (eqn 28a)
			float compA =  vec2f::dot(n, c1b);

			float c2 = vec2f::cross(rB, n) * 1.0f/Irect(bodyB, rB); // bodyB->iI;
			vec2f c2b = vec2f(-c2 * rB.y, c2 * rB.x);			// (eqn 28b)
			float compB = vec2f::dot(n, c2b);

			float j = (-1.0f - C) * vRelPre /
				(bodyA->imass + bodyB->imass + compA + compB);	// impulse length (eqn 28)

			vec2f J = n * j;									// impulse vector (eqn 24)
			//printf("(%f, %f)\n", J.x, J.y);

			// calculate friction component
			vec2f vAn = vec2f::projection(drdot, n); // n * vec2f::dot(drdot, n);
			vec2f vAt = drdot - vAn;

			float vAn_len = vAn.norm(), vAt_len = vAt.norm();
			float sfric = 0.4f;
			vec2f vAt_fComp;
			if(vAt_len <= (vAn_len * sfric)) {
				vAt_fComp = vAt;
				//printf("STAT\n");
			} else {
				vAt_fComp = vec2f(vAt).normalize() * vAn_len * bodyB->friction; // vec2f::getNorm(vAn * bodyB->friction);
				if(vAt_fComp.norm() > vAt_len)
					vAt_fComp = vAt;
				//printf("DYN\n");
			}
			vec2f JA = J - vAt_fComp;
			vec2f JB = -JA;

			// apply impulse to bodies
			//printf("%f, %f\n", JA.x, JA.y);
			//bodyA->applyImpulse(JA, rA);
			//bodyB->applyImpulse(JB, rB);

			distributeImpulse(bodyA, bodyB, JA, pocs, depths, totdepth);
			
			return true;
		} else
			return false;

	}

	vec2f calcImpulse(){}

	void distributeImpulse(t2dPoly *bodyA, t2dPoly *bodyB, vec2f J, std::list<vec2f> pocs, std::list<float> depths, float totdepth){
		vec2f dJ, rA, rB;
		std::list<float>::iterator dit = depths.begin(); //printf("%f, %f\n", J.x, J.y);

		if(totdepth < 1e-10)		// total depth is sometimes zero: abort
			return;

		float itotdepth = 1.0f / totdepth;
		for(std::list<vec2f>::iterator pit = pocs.begin(); pit != pocs.end(); pit++){
			//glBegin(GL_LINES); glVertex2d(bodyA->X.x, bodyA->X.y); glVertex2d(bodyA->X.x + pit->x, bodyA->X.y + pit->y); glEnd();
			dJ = J * (*dit * itotdepth); 
			rA = *pit - bodyA->X;
			rB = *pit - bodyB->X;
			bodyA->applyImpulse(dJ, rA);
			bodyB->applyImpulse(-dJ, rB);
			dit++;
		}
	}



	void render_GLtfm(t2World *world){
		t2Body* body;
		for(int i = 0; i < world->nbrBodies; i++){
			body = world->bodies[i];
			glPushMatrix();
			
			glTranslatef(body->X.x, body->X.y, 0);
			glRotatef(body->R, 0.f, 0.f, 1.f);
			//glTranslatef(-box->X.x, -box->X.y, 0); // vertices are already in body-space
			
			glBegin(GL_LINE_LOOP);
			for(int j = 0; j < body->nbrVertices; j++){
				glVertex2d(body->vertices[j].x, body->vertices[j].y);
			}
			glEnd();
			glPopMatrix();
		}
	}



	class t2dEulerInt {
	public:
		t2dEulerInt(void){}
	
		vec2f integrate(const vec2f &df, const float dt){
			return vec2f(df.x, df.y) * dt;
		}
	
		float integrate(const float &df, const float dt){
			return df * dt;
		}
	
		~t2dEulerInt(void){}
	};


#endif