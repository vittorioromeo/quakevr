// vr_menu_tunables.inc -- Debug > Tunables (a development build): every tuning constant (vr_tunables.inc) as the row it
// had on its page before it became one (vr_tunables_rows.inc: label, range, step, format, help), in submenus that mirror
// the menus they came from: Tunables > Combat > Armed Grunts > AI Behaviour. The submenus are made from the rows' group
// labels (QVR_TUNABLE_GROUP("Combat > Armed Grunts - AI Behaviour"): each part, at " > " or " - ", a page; a part named
// as the one above it, "Body > Body", is that page; one label in two places in the file is one page), none written by
// hand. A row sets the constant live (its stand-in cvar: Cvar_SetQuick -> tunableSet); what is set off its default is
// kept in tunables.cfg at quit (vr_tunables.cpp). Show: Changed Only lists the changed ones on the Tunables page itself;
// Search (the corner's button) finds every row and opens its submenu. Included by vr_menu_pages_debug.cpp.

#ifndef QVR_RELEASE

// A submenu of the Tunables page: its constants and the submenus below it.
struct TunableNode
{
    za::String name;
    int parent = -1;
    int depth = 0;            // 0: the Tunables page; a submenu's page is pageTunablesAt<depth>
    za::Vector<int> children; // nodes, in the order their groups first come in vr_tunables_rows.inc
    za::Vector<int> ids;      // TunableIds, in vr_tunables.inc's order
    int total = 0;            // constants here and below
};

constexpr int tunablesDepthMax = 4; // the submenus' pages: pageTunablesAt<1..4> (vr_menu_table.inc)

int tunablesPageKey = -1;            // the filter the page was built with (tunablesPageKeyNow)
bool tunablesPageUnfiltered = false; // Search's index: every row, whatever the filter
za::Vector<za::String> tunablesHelp; // each constant's help, its page and name after it (made once; rows point into it)
za::Vector<TunableNode> tunablesTree; // [0] the Tunables page (made once, with tunablesHelp)
za::Vector<int> tunablesNodeOf;       // each constant's node, by TunableId
int tunablesShown[tunablesDepthMax + 1]{}; // the node each depth's page shows (its link sets it as it opens the page)

int tunablesPageKeyNow()
{
    if(tunablesPageUnfiltered)
    {
        return -2;
    }
    return tunablesShowChangedCvar().value != 0.f ? 1 : 0;
}

void tunablesShowAll()
{
    Cvar_SetValueQuick(&tunablesShowChangedCvar(), 0.f);
}

[[nodiscard]] const char* tunablesSummary()
{
    return va("%d of %d set off their defaults (kept in tunables.cfg)", tunablesChanged(), static_cast<int>(tunableCount));
}

[[nodiscard]] const char* tunableTextLine(int id)
{
    return va("%s: \"%s\" (console: tune %s)", tunableRow(id).label, tunables[id].string, tunables[id].name);
}

void tunablesResetAllAction()
{
    tunablesResetAll();
}

[[nodiscard]] TunableNode& tunableNode(int node)
{
    return tunablesTree[static_cast<za::SizeT>(node)];
}

// The child of `node` named `name` (made if new).
[[nodiscard]] int tunablesChild(int node, const za::String& name)
{
    for(const int c : tunableNode(node).children)
    {
        if(tunableNode(c).name == name)
        {
            return c;
        }
    }
    TunableNode child;
    child.name = name;
    child.parent = node;
    child.depth = tunableNode(node).depth + 1;
    const int at = static_cast<int>(tunablesTree.size());
    tunablesTree.pushBack(ZA_MOVE(child));
    tunableNode(node).children.pushBack(at);
    return at;
}

// A group's label as its node: "Combat > Armed Grunts - AI Behaviour" is Combat, then Armed Grunts, then AI Behaviour.
[[nodiscard]] int tunablesNodeFor(const char* label)
{
    int node = 0;
    const char* s = label;
    while(*s)
    {
        const char* gt = strstr(s, " > ");
        const char* dash = strstr(s, " - ");
        const char* end = gt && (!dash || gt < dash) ? gt : dash;
        const za::String part = end ? za::String{s, static_cast<za::SizeT>(end - s)} : za::String{s};
        s = end ? end + 3 : s + strlen(s);
        if(part.empty() || part == tunableNode(node).name)
        {
            continue; // "Body > Body": the Body page's own
        }
        if(tunableNode(node).depth >= tunablesDepthMax)
        {
            Con_Printf("VR: Tunables: \"%s\" is deeper than %d pages: its rows on the last\n", label, tunablesDepthMax);
            break;
        }
        node = tunablesChild(node, part);
    }
    return node;
}

void tunablesMakeHelp()
{
    if(!tunablesHelp.empty())
    {
        return;
    }
    tunablesHelp.reserve(tunableCount);
    for(int id = 0; id < tunableCount; id++)
    {
        const TunableRow& r = tunableRow(id);
        za::String h{r.help ? r.help : ""};
        if(!h.empty())
        {
            h += ' ';
        }
        h += va("(%s; %s, default %s)", tunableGroup(id), tunables[id].name, tunableDefault(id));
        tunablesHelp.pushBack(ZA_MOVE(h));
    }
    TunableNode root;
    root.name = "Tunables";
    tunablesTree.pushBack(ZA_MOVE(root));
    tunablesNodeOf.resize(static_cast<za::SizeT>(tunableCount), 0);
    int count = 0;
    const TunableRow* rows = tunableRows(&count);
    int node = 0;
    for(int i = 0; i < count; i++)
    {
        if(rows[i].kind == TunableRow::Group)
        {
            node = tunablesNodeFor(rows[i].label);
            continue;
        }
        tunableNode(node).ids.pushBack(rows[i].id);
        tunablesNodeOf[static_cast<za::SizeT>(rows[i].id)] = node;
        for(int n = node; n >= 0; n = tunableNode(n).parent)
        {
            tunableNode(n).total++;
        }
    }
    // The order: the top pages as the Advanced VR Options list them (any other after), each page's "Other" last.
    const za::Vector<Item>& advanced = items(PageAdvanced);
    for(TunableNode& n : tunablesTree)
    {
        za::Vector<int> rank;
        for(const int c : n.children)
        {
            int r = static_cast<int>(advanced.size()) + 1;
            for(int i = 0; i < static_cast<int>(advanced.size()) && n.depth == 0; i++)
            {
                if(advanced[i].label && tunableNode(c).name == advanced[i].label)
                {
                    r = i;
                    break;
                }
            }
            rank.pushBack(tunableNode(c).name == "Other" ? r + 1 : r);
        }
        for(za::SizeT i = 1; i < n.children.size(); i++) // (insertion sort: stable, a few children)
        {
            for(za::SizeT j = i; j > 0 && rank[j - 1] > rank[j]; j--)
            {
                const int c = n.children[j], r = rank[j];
                n.children[j] = n.children[j - 1];
                rank[j] = rank[j - 1];
                n.children[j - 1] = c;
                rank[j - 1] = r;
            }
        }
    }
}

// A constant's row (its stand-in's slider or cycle; a text's line).
[[nodiscard]] Item tunableItem(int id)
{
    const TunableRow& r = tunableRow(id);
    cvar_t* var = tunableStandIn(id);
    const char* help = tunablesHelp[static_cast<za::SizeT>(id)].cStr();
    if(r.kind == TunableRow::Slider)
    {
        Item item = slider(r.label, var, r.min, r.max, r.step, r.format);
        if(r.hardMin < r.min || r.hardMax > r.max)
        {
            item = item.extend(r.hardMin, r.hardMax);
        }
        return item.help(help);
    }
    if(r.kind == TunableRow::Cycle)
    {
        za::Vector<Choice> choices;
        for(int c = 0; c < r.choiceCount; c++)
        {
            choices.pushBack({r.choices[c].value, r.choices[c].label});
        }
        return cycle(r.label, var, ZA_MOVE(choices)).help(help);
    }
    return infoLine(tunableTextLine, id);
}

// Whether node `inside` is `node` or below it.
[[nodiscard]] bool tunablesUnder(int inside, int node)
{
    for(int n = inside; n >= 0; n = tunableNode(n).parent)
    {
        if(n == node)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] const char* tunablesLinkText(int node)
{
    const TunableNode& n = tunableNode(node);
    int changed = 0;
    for(int id = 0; id < tunableCount; id++)
    {
        changed += tunableChanged(id) && tunablesUnder(tunablesNodeOf[static_cast<za::SizeT>(id)], node) ? 1 : 0;
    }
    return changed ? va("%s (%d, %d changed)", n.name.cStr(), n.total, changed) : va("%s (%d)", n.name.cStr(), n.total);
}

[[nodiscard]] const char* tunablesLinkHelp(int node)
{
    const TunableNode& n = tunableNode(node);
    return va("The tuning constants from %s's rows and the pages below it (%d).", n.name.cStr(), n.total);
}

void tunablesPick(int node)
{
    tunablesShown[tunableNode(node).depth] = node;
}

// The page of a depth: the Tunables page (0), a submenu's (1..tunablesDepthMax).
[[nodiscard]] int tunablesPageAt(int depth)
{
    constexpr int pagesAt[tunablesDepthMax + 1] = {pageIndex(pageTunables), pageIndex(pageTunablesAt<1>),
        pageIndex(pageTunablesAt<2>), pageIndex(pageTunablesAt<3>), pageIndex(pageTunablesAt<4>)};
    return pagesAt[q_max(0, q_min(depth, tunablesDepthMax))];
}

// `node`'s submenus' links, then its constants.
void tunablesNodeRows(za::Vector<Item>& list, int node)
{
    const TunableNode& n = tunableNode(node);
    for(const int c : n.children)
    {
        list.pushBack(row(tunablesLinkText, tunablesLinkHelp, tunablesPick, c, tunablesPageAt(n.depth + 1)));
    }
    if(!n.ids.empty())
    {
        if(!n.children.empty())
        {
            list.pushBack(header(n.name.cStr()));
        }
        for(const int id : n.ids)
        {
            list.pushBack(tunableItem(id));
        }
    }
}

[[nodiscard]] const char* tunablesNoPage()
{
    return "Open a page from Debug > Tunables.";
}

// A submenu's page (its title the submenu's name: tunablesTitle).
template<int Depth>
[[nodiscard]] za::Vector<Item> pageTunablesAt()
{
    tunablesMakeHelp();
    za::Vector<Item> list;
    const int node = tunablesShown[Depth];
    if(node > 0 && node < static_cast<int>(tunablesTree.size()) && tunableNode(node).depth == Depth)
    {
        tunablesNodeRows(list, node);
    }
    else
    {
        list.pushBack(info(tunablesNoPage));
    }
    return list;
}

template za::Vector<Item> pageTunablesAt<1>();
template za::Vector<Item> pageTunablesAt<2>();
template za::Vector<Item> pageTunablesAt<3>();
template za::Vector<Item> pageTunablesAt<4>();

const char* tunablesTitle(int page)
{
    for(int depth = 1; depth <= tunablesDepthMax; depth++)
    {
        const int node = tunablesShown[depth];
        if(page == tunablesPageAt(depth) && node > 0 && node < static_cast<int>(tunablesTree.size()))
        {
            return tunableNode(node).name.cStr();
        }
    }
    return nullptr;
}

int tunablesSearchTarget(int target, const cvar_t* cvar)
{
    tunablesMakeHelp();
    tunablesShowAll();
    const cvar_t* first = tunableStandIn(0);
    if(!cvar || cvar < first || cvar >= first + tunableCount)
    {
        return target;
    }
    const int node = tunablesNodeOf[static_cast<za::SizeT>(cvar - first)];
    for(int n = node; n > 0; n = tunableNode(n).parent)
    {
        tunablesPick(n);
    }
    return tunablesPageAt(tunableNode(node).depth);
}

void tunablesTreeCheck_f()
{
    tunablesMakeHelp();
    int reached = 0, twice = 0, deepest = 0, withRows = 0;
    za::Vector<int> seen(static_cast<za::SizeT>(tunableCount), 0);
    for(const TunableNode& n : tunablesTree)
    {
        deepest = q_max(deepest, n.depth);
        withRows += n.ids.empty() ? 0 : 1;
        for(const int id : n.ids)
        {
            seen[static_cast<za::SizeT>(id)]++;
        }
        if(Cmd_Argc() > 1)
        {
            Con_Printf("%*s%s: %d here, %d in all\n", n.depth * 2, "", n.name.cStr(), static_cast<int>(n.ids.size()), n.total);
        }
    }
    for(const int s : seen)
    {
        reached += s > 0 ? 1 : 0;
        twice += s > 1 ? 1 : 0;
    }
    const bool ok = reached == tunableCount && twice == 0 && tunablesTree[0].total == tunableCount &&
                    deepest <= tunablesDepthMax;
    Con_Printf("tunables tree: %d pages (%d with rows), %d of %d constants reached, %d twice, %d deep: %s\n",
        static_cast<int>(tunablesTree.size()) - 1, withRows, reached, static_cast<int>(tunableCount), twice, deepest,
        ok ? "PASS" : "FAIL");
}

[[nodiscard]] za::Vector<Item> pageTunables()
{
    tunablesMakeHelp();
    tunablesPageKey = tunablesPageKeyNow();
    const bool changedOnly = !tunablesPageUnfiltered && tunablesShowChangedCvar().value != 0.f;
    za::Vector<Item> list;
    list.pushBackMultiple(info(tunablesSummary),
        cycle("Show", tunablesShowChangedCvar(), {{0.f, "Pages"}, {1.f, "Changed Only"}})
            .help("The tuning constants in pages like the menus they came from, or every one set off its default, here."),
        action("Reset All to Defaults", tunablesResetAllAction)
            .help("Every tuning constant back to its default (tune_reset). Kept in tunables.cfg at quit: an empty one is "
                  "removed."));
    if(!tunablesPageUnfiltered && !changedOnly)
    {
        list.pushBack(header("Pages"));
        tunablesNodeRows(list, 0);
        return list;
    }
    // Every row (Search's index: a result opens its submenu, tunablesSearchTarget) or the changed ones, under their
    // groups' headers.
    int count = 0;
    const TunableRow* rows = tunableRows(&count);
    const char* pendingHeader = nullptr;
    for(int i = 0; i < count; i++)
    {
        const TunableRow& r = rows[i];
        if(r.kind == TunableRow::Group)
        {
            pendingHeader = r.label;
            continue;
        }
        if(changedOnly && !tunableChanged(r.id))
        {
            continue;
        }
        if(pendingHeader)
        {
            list.pushBack(header(pendingHeader));
            pendingHeader = nullptr;
        }
        list.pushBack(tunableItem(r.id));
    }
    return list;
}

#endif
