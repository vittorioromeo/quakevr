// Frozen pre-optimization shadow collection oracle (d986e7439).
void addSurface(const msurface_t* s)
{
    if(s->flags & (SURF_DRAWSKY | SURF_DRAWTURB))
    {
        return;
    }
    for(int k = 2; k < s->numedges; k++)
    {
        indices.pushBack(static_cast<uint32_t>(s->vbo_firstvert));
        indices.pushBack(static_cast<uint32_t>(s->vbo_firstvert + k - 1));
        indices.pushBack(static_cast<uint32_t>(s->vbo_firstvert + k));
    }
}

void collectWorld(const mnode_t* node, const glm::vec3& p, float radius)
{
    while(node->contents >= 0)
    {
        const mplane_t* plane = node->plane;
        const float d = p.x * plane->normal[0] + p.y * plane->normal[1] + p.z * plane->normal[2] - plane->dist;
        if(d > radius)
        {
            node = node->children[0];
            continue;
        }
        if(d < -radius)
        {
            node = node->children[1];
            continue;
        }
        const msurface_t* s = cl.worldmodel->surfaces + node->firstsurface;
        for(unsigned i = 0; i < node->numsurfaces; i++, s++)
        {
            if(p.x + radius < s->mins[0] || p.x - radius > s->maxs[0] || p.y + radius < s->mins[1] ||
                p.y - radius > s->maxs[1] || p.z + radius < s->mins[2] || p.z - radius > s->maxs[2])
            {
                continue;
            }
            addSurface(s);
        }
        collectWorld(node->children[0], p, radius);
        node = node->children[1];
    }
}

bool touches(const entity_t* e, const glm::vec3& light, float radius)
{
    const glm::vec3 lo{e->model->mins[0], e->model->mins[1], e->model->mins[2]};
    const glm::vec3 hi{e->model->maxs[0], e->model->maxs[1], e->model->maxs[2]};
    const float r = za::max(glm::length(lo), glm::length(hi)) * VR_EntityScale(e);
    return glm::distance(glm::vec3{e->origin[0], e->origin[1], e->origin[2]}, light) < radius + r;
}

void collectBrushes(const glm::vec3& light, float radius, bool itemsOnly)
{
    brushCasters.clear();
    for(int i = 1; i < cl.num_entities; i++)
    {
        entity_t& e = cl_entities[i];
        // (the boxes made in the game are not in the baked light either: vr_brushes.cpp)
        if(!e.model || e.model->type != mod_brush || e.model == cl.worldmodel || e.msgtime != cl.mtime[0] ||
            (itemsOnly && qvr::modelmeta::has(e.model, qvr::modelmeta::Trait::Submodel) && !qvr::brushes::isRuntime(e.model)) ||
            !touches(&e, light, radius))
        {
            continue;
        }
        const size_t first = indices.size();
        const msurface_t* s = e.model->surfaces + e.model->firstmodelsurface;
        for(int k = 0; k < e.model->nummodelsurfaces; k++, s++)
        {
            addSurface(s);
        }
        if(indices.size() > first)
        {
            brushCasters.pushBack({&e, first, indices.size() - first});
        }
    }
    // A box put away, shrinking into its holster (vr_collectfx.cpp): it cast its shadow in the hand a moment ago.
    for(int c = 0; c < collectfx::maxCopies; c++)
    {
        entity_t* e = collectfx::liveCopy(c);
        if(!e || e->model->type != mod_brush || qvr::modelmeta::has(e->model, qvr::modelmeta::Trait::Submodel) ||
            !touches(e, light, radius))
        {
            continue;
        }
        const size_t first = indices.size();
        const msurface_t* s = e->model->surfaces + e->model->firstmodelsurface;
        for(int k = 0; k < e->model->nummodelsurfaces; k++, s++)
        {
            addSurface(s);
        }
        if(indices.size() > first)
        {
            brushCasters.pushBack({e, first, indices.size() - first});
        }
    }
}
