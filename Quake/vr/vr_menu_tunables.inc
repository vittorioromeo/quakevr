// vr_menu_tunables.inc -- Debug > Tunables (a development build): every tuning constant (vr_tunables.inc) as the row it
// had on its page before it became one (vr_tunables_rows.inc: label, range, step, format, help), grouped under the
// pages they came from. A row sets the constant live (its stand-in cvar: Cvar_SetQuick -> tunableSet); what is set off
// its default is kept in tunables.cfg at quit (vr_tunables.cpp). Show and Page filter the list; Search (the corner's
// button) finds the rows too (opening one shows the whole list on it). Included by vr_menu_pages_debug.cpp.

#ifndef QVR_RELEASE

int tunablesPageKey = -1;            // the filter the page was built with (tunablesPageKeyNow)
bool tunablesPageUnfiltered = false; // Search's index: every row, whatever the filter
za::Vector<za::String> tunablesHelp; // each constant's help, its page and name after it (made once; rows point into it)
za::Vector<Choice> tunablesPageChoices;

int tunablesPageKeyNow()
{
    if(tunablesPageUnfiltered)
    {
        return -2;
    }
    return static_cast<int>(tunablesPageCvar().value) * 2 + (tunablesShowChangedCvar().value != 0.f ? 1 : 0);
}

void tunablesShowAll()
{
    Cvar_SetValueQuick(&tunablesPageCvar(), 0.f);
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
    int count = 0;
    const TunableRow* rows = tunableRows(&count);
    tunablesPageChoices.pushBack({0.f, "All"});
    float n = 0.f;
    for(int i = 0; i < count; i++)
    {
        if(rows[i].kind == TunableRow::Group)
        {
            tunablesPageChoices.pushBack({++n, rows[i].label});
        }
    }
}

[[nodiscard]] za::Vector<Item> pageTunables()
{
    tunablesMakeHelp();
    tunablesPageKey = tunablesPageKeyNow();
    const int only = tunablesPageUnfiltered ? 0 : static_cast<int>(tunablesPageCvar().value);
    const bool changedOnly = !tunablesPageUnfiltered && tunablesShowChangedCvar().value != 0.f;
    za::Vector<Item> list;
    list.pushBackMultiple(info(tunablesSummary),
        cycle("Show", tunablesShowChangedCvar(), {{0.f, "All"}, {1.f, "Changed Only"}})
            .help("All the tuning constants, or only the ones set off their defaults."),
        cycle("Page", tunablesPageCvar(), tunablesPageChoices)
            .help("The constants of one page (where their rows were before they became constants), or all. Search "
                  "(the corner's button) finds them by name, label or help too."),
        action("Reset All to Defaults", tunablesResetAllAction)
            .help("Every tuning constant back to its default (tune_reset). Kept in tunables.cfg at quit: an empty one is "
                  "removed."));
    int count = 0;
    const TunableRow* rows = tunableRows(&count);
    int group = 0;
    const char* pendingHeader = nullptr;
    for(int i = 0; i < count; i++)
    {
        const TunableRow& r = rows[i];
        if(r.kind == TunableRow::Group)
        {
            group++;
            pendingHeader = r.label;
            continue;
        }
        if((only && group != only) || (changedOnly && !tunableChanged(r.id)))
        {
            continue;
        }
        if(pendingHeader)
        {
            list.pushBack(header(pendingHeader));
            pendingHeader = nullptr;
        }
        cvar_t* var = tunableStandIn(r.id);
        const char* help = tunablesHelp[r.id].cStr();
        if(r.kind == TunableRow::Slider)
        {
            Item item = slider(r.label, var, r.min, r.max, r.step, r.format);
            if(r.hardMin < r.min || r.hardMax > r.max)
            {
                item = item.extend(r.hardMin, r.hardMax);
            }
            list.pushBack(item.help(help));
        }
        else if(r.kind == TunableRow::Cycle)
        {
            za::Vector<Choice> choices;
            for(int c = 0; c < r.choiceCount; c++)
            {
                choices.pushBack({r.choices[c].value, r.choices[c].label});
            }
            list.pushBack(cycle(r.label, var, ZA_MOVE(choices)).help(help));
        }
        else
        {
            list.pushBack(infoLine(tunableTextLine, r.id));
        }
    }
    return list;
}

#endif
