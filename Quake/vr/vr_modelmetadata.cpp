// vr_modelmetadata.cpp -- shared main-thread cache; string-only identification is pure.
#include "vr_modelmetadata.hpp"
#include "vr_retro.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"
#include "vr_zancle.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::modelmeta
{
namespace
{
using retro::Category;
[[nodiscard]] bool startsWith(const char* s, const char* prefix)
{
    return ZA_STRNCMP(s, prefix, ZA_STRLEN(prefix)) == 0;
}

[[nodiscard]] const char* fileOf(const char* name)
{
    const char* f = name;
    for(const char* p = name; *p; p++)
    {
        if(*p == '/')
        {
            f = p + 1;
        }
    }
    return f;
}

// An owned expansion's file read in place ("owned/mg3/progs/v_hammer.mdl": vr_gamedir.cpp VR_OwnedFile): the path it has
// in its own pack ("progs/v_hammer.mdl"), whose traits it has (a view weapon, ...); its identity stays its full name.
[[nodiscard]] const char* packPathOf(const char* name)
{
    if(!startsWith(name, "owned/"))
    {
        return name;
    }
    const char* slash = strchr(name + 6, '/');
    return slash ? slash + 1 : name;
}

template <za::SizeT N>
[[nodiscard]] bool anyPrefix(const char* f, const char* const (&prefixes)[N])
{
    for(const char* p : prefixes)
    {
        if(startsWith(f, p))
        {
            return true;
        }
    }
    return false;
}

constexpr const char* gibPrefixes[] = {"gib1.", "gib2.", "gib3.", "gib_brain", "zom_gib.", "statgib", "h_"};
constexpr const char* handPrefixes[] = {"hand", "finger_"};
constexpr const char* gearPrefixes[] = {"vrgadget", "vrpauldron", "vrpouch", "legholster", "vrflashlight"};
constexpr const char* propPrefixes[] = {"vr_crate", "vr_rock", "vr_brick", "vr_plank", "vr_shell", "vrtorch.", "lantern",
    "candle", "barrel", "vr_barrel"};
// Quake's monsters, the mission packs' (hipnotic: scorpions, gremlins, the armagon; rogue: mummies, eels, lava men,
// dragons) and the player's model (other players, statues)
constexpr const char* monsterFiles[] = {"soldier.", "dog.", "ogre.", "knight.", "hknight.", "wizard.", "demon.",
    "shambler.", "zombie.", "shalrath.", "enforcer.", "fish.", "tarbaby.", "boss.", "oldone.", "player.", "scor.",
    "grem.", "armabody.", "armalegs.", "mummy.", "eel.", "lavaman.", "dragon.", "ogre_", "sword.", "frogman",
    "dog_explosive.", "rknight.", "teleporter_eye", "shambler_blood."}; // (the last four Dawn of the Machine's: read in place, owned/mg3/progs/)
constexpr const char* itemFiles[] = {"armor.", "backpack.", "w_s_key.", "w_g_key.", "m_s_key.", "m_g_key.", "b_s_key.",
    "b_g_key.", "invulner.", "suit.", "invisibl.", "quaddama.", "end1.", "end2.", "end3.", "end4.", "empathy.",
    "wetsuit.", "shield.", "antigrav."};

// A model's category by its name alone (entities that move it elsewhere: categoryOf).
[[nodiscard]] Category classifyModel(const qmodel_t* m)
{
    const char* f = fileOf(m->name);
    if(m->type == mod_brush)
    {
        if(m->name[0] == '*')
        {
            return Category::Brush;
        }
        if(startsWith(f, "b_explob") || startsWith(f, "b_exbox"))
        {
            return Category::Props;
        }
        return startsWith(f, "b_") ? Category::Items : Category::Props;
    }
    if(m->type == mod_sprite)
    {
        return Category::Sprites;
    }
    if(m->type != mod_alias)
    {
        return Category::Other;
    }
    if(anyPrefix(f, gibPrefixes))
    {
        return Category::Gibs;
    }
    if(startsWith(f, "v_") || startsWith(f, "g_"))
    {
        return Category::Weapons;
    }
    if(startsWith(f, "vrbody"))
    {
        return Category::Torso; // (by part: bodyParts)
    }
    if(anyPrefix(f, handPrefixes))
    {
        return Category::Hands;
    }
    if(anyPrefix(f, gearPrefixes))
    {
        return Category::Gear;
    }
    if(anyPrefix(f, propPrefixes))
    {
        return Category::Props;
    }
    if(anyPrefix(f, monsterFiles))
    {
        return Category::Monsters;
    }
    if((m->flags & EF_ROTATE) || anyPrefix(f, itemFiles))
    {
        return Category::Items;
    }
    return Category::Other;
}

[[nodiscard]] Category aliasCategory(const char* f)
{
    if(anyPrefix(f, gibPrefixes))
    {
        return Category::Gibs;
    }
    if(startsWith(f, "v_") || startsWith(f, "g_"))
    {
        return Category::Weapons;
    }
    if(startsWith(f, "vrbody"))
    {
        return Category::Torso; // (by part: bodyParts)
    }
    if(anyPrefix(f, handPrefixes))
    {
        return Category::Hands;
    }
    if(anyPrefix(f, gearPrefixes))
    {
        return Category::Gear;
    }
    if(anyPrefix(f, propPrefixes))
    {
        return Category::Props;
    }
    if(anyPrefix(f, monsterFiles))
    {
        return Category::Monsters;
    }
    if(anyPrefix(f, itemFiles))
    {
        return Category::Items;
    }
    return Category::Other;
}

// Your body's bones by their names (make_vrbody.py's, vr_avatar.cpp's jointNames): the hands' and wrists', the arms',
// the legs'; the rest (pelvis, spine, chest, neck, head, clavicles) the torso's.
constexpr const char* handBones[] = {"hand_", "wrist_", "finger", "thumb"};
constexpr const char* armBones[] = {"upperarm_", "forearm_", "foretwist"};
constexpr const char* legBones[] = {"thigh_", "calf_", "foot_", "toe"};

[[nodiscard]] Category bonePart(const char* name)
{
    if(anyPrefix(name, handBones))
    {
        return Category::Hands;
    }
    if(anyPrefix(name, armBones))
    {
        return Category::Arms;
    }
    return anyPrefix(name, legBones) ? Category::Legs : Category::Torso;
}

// Your body's parts (progs/vrbody*, skinned): each bone's part (Hands .. Legs: 0 .. 3) as two bits, as InstanceData's
// RetroPart has them: xy the low bits of bones 0..23 and 24..47, zw the high bits, as whole numbers (a vertex's part is
// its heaviest bone's). False (all 0) for any other model, or a body without bones: drawn whole by its category.
bool makeBodyParts(const qmodel_t* model, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!model || model->type != mod_alias || !startsWith(fileOf(model->name), "vrbody"))
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones <= 0 || !hdr->boneinfo)
    {
        return false;
    }
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    za::U32 bits[4]{0u, 0u, 0u, 0u};
    for(int i = 0; i < za::min(hdr->numbones, 48); i++)
    {
        const int part = static_cast<int>(bonePart(bones[i].name)) - static_cast<int>(Category::Hands);
        if(part & 1)
        {
            bits[i / 24] |= 1u << (i % 24);
        }
        if(part & 2)
        {
            bits[2 + i / 24] |= 1u << (i % 24);
        }
    }
    for(int k = 0; k < 4; k++)
    {
        out[k] = static_cast<float>(bits[k]);
    }
    return true;
}


void makeRightBones(const qmodel_t* model, float out[2])
{
    out[0] = out[1] = 0.f;
    if(!model || model->type != mod_alias || !modelmeta::has(model, modelmeta::Trait::Body))
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones <= 0 || !hdr->boneinfo)
    {
        return;
    }
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    za::U32 bits[2]{0u, 0u};
    for(int i = 0; i < za::min(hdr->numbones, 48); i++)
    {
        const za::SizeT n = strlen(bones[i].name);
        if(n > 2 && bones[i].name[n - 2] == '_' && bones[i].name[n - 1] == 'r')
        {
            bits[i / 24] |= 1u << (i % 24);
        }
    }
    out[0] = static_cast<float>(bits[0]);
    out[1] = static_cast<float>(bits[1]);
}
constexpr const char* paths[] = {
    "",
#define QVR_MODEL_ID(name, path) path,
#define QVR_MODEL_TRAIT(name, match, text)
#include "vr_modelmetadata.inc"
#undef QVR_MODEL_ID
#undef QVR_MODEL_TRAIT
};
static_assert(za::getArraySize(paths) == static_cast<int>(Id::Count));
// Immutable open-addressed name table: string-only hooks can query it on workers without lazy initialisation.
constexpr za::U32 nameHash(const char* name)
{
    za::U32 hash = 2166136261u;
    for(; *name; ++name) { hash = (hash ^ static_cast<unsigned char>(*name)) * 16777619u; }
    return hash;
}
constexpr int nameSlots = 256;
static_assert(static_cast<int>(Id::Count) * 2 < nameSlots);
constexpr auto nameIds = [] {
    za::Array<Id, nameSlots> result{};
    for(int i = 1; i < static_cast<int>(Id::Count); i++)
    {
        int slot = nameHash(paths[i]) & (nameSlots - 1);
        while(result[slot] != Id::Unknown) { slot = (slot + 1) & (nameSlots - 1); }
        result[slot] = static_cast<Id>(i);
    }
    return result;
}();
enum class Match { Prefix, Contains, FilePrefix, Suffix, PrefixInsensitive, ExactInsensitive };
struct Rule { Match match; const char* text; };
constexpr Rule rules[] = {
#define QVR_MODEL_ID(name, path)
#define QVR_MODEL_TRAIT(name, match, text) {Match::match, text},
#include "vr_modelmetadata.inc"
#undef QVR_MODEL_ID
#undef QVR_MODEL_TRAIT
};
static_assert(za::getArraySize(rules) == static_cast<int>(Trait::Count));
struct BoneIndices { int first{-1}, last{-1}; };

struct CachedModel
{
    ModelMetadata info;
    Category alias{Category::Other};
    bool ready = false, partsReady = false, splitBody = false, bonesReady = false, rightReady = false;
    ankerl::unordered_dense::map<za::String, BoneIndices> bones;
    float parts[4]{}, right[2]{};
};
za::SizeT heldBytes(const CachedModel& model) { return mem::heldBytes(model.bones); }
struct Models
{
    ankerl::unordered_dense::map<const qmodel_t*, za::UniquePtr<CachedModel>> models;
    auto members() { return mem::list(models); }
};
mem::Cache<Models> cache{"model metadata", mem::MapChange | mem::GameDirChange | mem::ModelReload};
const ModelMetadata empty{};

CachedModel& cached(const qmodel_t* model)
{
    CachedModel& memo = qza::stableAt<CachedModel>(cache.models, model);
    if(!memo.ready)
    {
        memo.info = describePath(model->name);
        memo.alias = aliasCategory(fileOf(model->name));
        memo.ready = true;
    }
    return memo;
}
} // namespace

Id identifyPath(const char* name)
{
    if(name)
    {
        int slot = nameHash(name) & (nameSlots - 1);
        while(nameIds[slot] != Id::Unknown)
        {
            const Id id = nameIds[slot];
            if(!strcmp(name, paths[static_cast<int>(id)])) { return id; }
            slot = (slot + 1) & (nameSlots - 1);
        }
    }
    return Id::Unknown;
}

const char* path(Id id)
{
    const int index = static_cast<int>(id);
    return index > 0 && index < static_cast<int>(Id::Count) ? paths[index] : "";
}

ModelMetadata describePath(const char* name)
{
    ModelMetadata result = empty;
    if(!name) { return result; }
    result.id = identifyPath(name);
    name = packPathOf(name);
    const char* file = fileOf(name);
    const size_t n = strlen(name);
    for(int i = 0; i < static_cast<int>(Trait::Count); i++)
    {
        const Rule& rule = rules[i];
        const size_t len = strlen(rule.text);
        const bool match = rule.match == Match::ExactInsensitive ? !q_strcasecmp(name, rule.text) :
            rule.match == Match::PrefixInsensitive ? !q_strncasecmp(name, rule.text, len) :
            rule.match == Match::Contains ? strstr(name, rule.text) != nullptr :
            rule.match == Match::Suffix ? n >= len && !strcmp(name + n - len, rule.text) :
            startsWith(rule.match == Match::FilePrefix ? file : name, rule.text);
        if(match) { result.traits |= za::U64{1} << i; }
    }
    return result;
}

const ModelMetadata& get(const qmodel_t* model)
{
    return model ? cached(model).info : empty;
}

retro::Category category(const qmodel_t* model)
{
    if(!model) { return Category::Other; }
    const CachedModel& memo = cached(model);
    const ModelMetadata& info = memo.info;
    Category result = Category::Other;
    if(model->type == mod_brush)
    {
        result = info.has(Trait::Submodel) ? Category::Brush :
            info.has(Trait::ExplosiveBoxFile) || info.has(Trait::ExplosiveCrateFile) ? Category::Props :
            info.has(Trait::BrushItemFile) ? Category::Items : Category::Props;
    }
    else if(model->type == mod_sprite) { result = Category::Sprites; }
    else if(model->type == mod_alias)
    {
        result = memo.alias;
        if(result == Category::Other && (model->flags & EF_ROTATE)) { result = Category::Items; }
    }
    if(vr_prop_query_verify.value && result != classifyModel(model))
    {
        Sys_Error("model metadata category changed %s", model->name);
    }
    return result;
}

bool bodyParts(const qmodel_t* model, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!model || model->type != mod_alias || !has(model, Trait::BodyFile)) { return false; }
    // Load before retaining the cache entry: alias data can be evicted and loading resets derived metadata.
    Mod_Extradata(const_cast<qmodel_t*>(model));
    CachedModel& memo = cached(model);
    if(!memo.partsReady)
    {
        memo.splitBody = makeBodyParts(model, memo.parts);
        memo.partsReady = true;
    }
    if(vr_prop_query_verify.value)
    {
        float expected[4];
        if(makeBodyParts(model, expected) != memo.splitBody || memcmp(expected, memo.parts, sizeof(expected)))
        {
            Sys_Error("model metadata body parts changed %s", model->name);
        }
    }
    ZA_MEMCPY(out, memo.parts, sizeof(memo.parts));
    return memo.splitBody;
}

void rightBones(const qmodel_t* model, float out[2])
{
    out[0] = out[1] = 0.f;
    if(!model || model->type != mod_alias || !has(model, Trait::Body)) { return; }
    Mod_Extradata(const_cast<qmodel_t*>(model));
    CachedModel& memo = cached(model);
    if(!memo.rightReady)
    {
        makeRightBones(model, memo.right);
        memo.rightReady = true;
    }
    if(vr_prop_query_verify.value)
    {
        float expected[2]; makeRightBones(model, expected);
        if(memcmp(expected, memo.right, sizeof(expected))) { Sys_Error("model metadata right bones changed %s", model->name); }
    }
    ZA_MEMCPY(out, memo.right, sizeof(memo.right));
}

int boneIndex(const qmodel_t* model, const char* name, bool last)
{
    if(!model || !name || model->type != mod_alias) { return -1; }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones <= 0 || !hdr->boneinfo) { return -1; }
    CachedModel& memo = cached(model);
    if(!memo.bonesReady)
    {
        const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
        for(int i = 0; i < hdr->numbones; i++)
        {
            auto [it, inserted] = memo.bones.try_emplace(za::String{bones[i].name}, BoneIndices{i, i});
            if(!inserted) { it->second.last = i; }
        }
        memo.bonesReady = true;
    }
    const auto it = memo.bones.find(za::String{name});
    return it == memo.bones.end() ? -1 : last ? it->second.last : it->second.first;
}

BoneRole boneRole(const char* name)
{
    if(name && !strcmp(name, "head")) { return BoneRole::Head; }
    if(name && !strcmp(name, "chest")) { return BoneRole::Chest; }
    return BoneRole::Other;
}

void changed(const qmodel_t* model)
{
    const auto it = cache.models.find(model);
    if(it != cache.models.end()) { *it->second = CachedModel{}; }
}

void test_f()
{
    int checks = 0, models = 0;
    const auto require = [&](bool ok, const char* what) {
        ++checks;
        if(!ok) { Sys_Error("modelmetadata test failed: %s", what); }
    };
    require(get(nullptr).id == Id::Unknown && get(nullptr).traits == 0 &&
        !get(nullptr).is(Id::Unknown) && category(nullptr) == Category::Other, "null/unknown");
    require(identifyPath(nullptr) == Id::Unknown && describePath(nullptr).traits == 0, "null path");
    qmodel_t fixture{};
    const auto checkPath = [&](const char* name) {
        q_strlcpy(fixture.name, name, sizeof(fixture.name));
        changed(&fixture);
        const auto& info = get(&fixture);
        for(int i = 1; i < static_cast<int>(Id::Count); i++)
        {
            require(info.is(static_cast<Id>(i)) == !strcmp(fixture.name, paths[i]), "exact identity");
        }
        // Independent legacy operations, evaluated directly against the original model name.
        for(int i = 0; i < static_cast<int>(Trait::Count); i++)
        {
            const Rule& rule = rules[i];
            const size_t n = strlen(fixture.name), len = strlen(rule.text);
            const bool expected = rule.match == Match::ExactInsensitive ? q_strcasecmp(fixture.name, rule.text) == 0 :
                rule.match == Match::Contains ? strstr(fixture.name, rule.text) != nullptr :
                rule.match == Match::Suffix ? n >= len && strcmp(fixture.name + n - len, rule.text) == 0 :
                rule.match == Match::PrefixInsensitive ? q_strncasecmp(fixture.name, rule.text, len) == 0 :
                strncmp(rule.match == Match::FilePrefix ? fileOf(fixture.name) : fixture.name, rule.text, len) == 0;
            require(info.has(static_cast<Trait>(i)) == expected, "legacy path predicate");
        }
        for(const auto type : {mod_alias, mod_brush, mod_sprite})
        {
            fixture.type = type;
            for(const int flags : {0, EF_ROTATE, EF_GIB | EF_ROTATE})
            {
                fixture.flags = flags;
                require(category(&fixture) == classifyModel(&fixture), "live type/flags");
            }
        }
    };
    char text[MAX_QPATH];
    for(const char* name : paths)
    {
        checkPath(name);
        q_snprintf(text, sizeof(text), "%s#rag", name); checkPath(text);
        q_snprintf(text, sizeof(text), "%s.extra", name); checkPath(text);
        q_strlcpy(text, name, sizeof(text)); if(text[0]) { text[0] = 'P'; } checkPath(text);
    }
    for(const Rule& rule : rules)
    {
        checkPath(rule.text);
        q_snprintf(text, sizeof(text), "%s_custom.mdl", rule.text); checkPath(text);
        q_snprintf(text, sizeof(text), "custom/%s", rule.text); checkPath(text);
        q_strlcpy(text, rule.text, sizeof(text));
        const size_t n = strlen(text); if(n) { text[n - 1] = 0; } checkPath(text);
    }
    for(const char* name : {"*1", "models/vrbody_custom.mdl", "progs/handmade.mdl", "progs/vr_rock_custom.mdl",
        "custom/armor_backpack_key_gib_vrbody.mdl", "MAPS/B_shell0.bsp", "progs/finger_", "progs/notknown.mdl"})
    {
        checkPath(name);
    }
    require(describePath("progs/flame.mdl#rag").id == Id::Unknown, "synthetic is not base identity");
    require(boneRole("head") == BoneRole::Head && boneRole("chest") == BoneRole::Chest &&
        boneRole("head_extra") == BoneRole::Other, "rig roles");
    // A retained reference must survive dense-map reallocation and erasure of other keys.
    const ModelMetadata* retained = &get(&fixture);
    za::Vector<za::UniquePtr<qmodel_t>> growth;
    for(int i = 0; i < 512; i++)
    {
        auto m = za::makeUnique<qmodel_t>();
        q_snprintf(m->name, sizeof(m->name), "progs/metadata_growth_%d.mdl", i);
        (void)get(m.get());
        growth.pushBack(ZA_MOVE(m));
    }
    require(retained == &get(&fixture), "stable references on growth");
    for(const auto& m : growth) { cache.models.erase(m.get()); }
    require(retained == &get(&fixture), "stable references on erase");
    // A tiny real engine-cache skeleton checks duplicate-name policy, two-bit packing and reload invalidation.
    q_strlcpy(fixture.name, "progs/vrbody_metadata_test.mdl", sizeof(fixture.name));
    fixture.type = mod_alias; fixture.needload = false; changed(&fixture);
    constexpr int boneCount = 50;
    const int bytes = sizeof(aliashdr_t) + boneCount * sizeof(boneinfo_t);
    auto* hdr = static_cast<aliashdr_t*>(Cache_Alloc(&fixture.cache, bytes, fixture.name));
    memset(hdr, 0, bytes);
    hdr->poseverttype = aliashdr_t::PV_IQM; hdr->numbones = boneCount; hdr->boneinfo = sizeof(aliashdr_t);
    auto* bones = reinterpret_cast<boneinfo_t*>(reinterpret_cast<byte*>(hdr) + hdr->boneinfo);
    for(int i = 0; i < boneCount; i++)
    {
        strcpy(bones[i].name, i % 4 == 0 ? "hand_l" : i % 4 == 1 ? "upperarm_r" : i % 4 == 2 ? "chest" : "thigh_r");
    }
    float parts[4], expected[4];
    require(bodyParts(&fixture, parts) && makeBodyParts(&fixture, expected) &&
        !memcmp(parts, expected, sizeof(parts)), "48-bone masks");
    require(boneIndex(&fixture, "hand_l") == 0 && boneIndex(&fixture, "hand_l", true) == 48 &&
        boneIndex(&fixture, "absent") == -1, "bone map duplicate policy");
    float right[2], rightExpected[2];
    rightBones(&fixture, right); makeRightBones(&fixture, rightExpected);
    require(!memcmp(right, rightExpected, sizeof(right)), "right-side masks");
    strcpy(bones[0].name, "thigh_r"); changed(&fixture);
    rightBones(&fixture, right); makeRightBones(&fixture, rightExpected);
    require(!memcmp(right, rightExpected, sizeof(right)), "reload invalidates right mask");
    require(boneIndex(&fixture, "hand_l") == 4 && bodyParts(&fixture, parts) &&
        makeBodyParts(&fixture, expected) && !memcmp(parts, expected, sizeof(parts)), "reload invalidates bones/masks");
    Cache_Free(&fixture.cache, true);
    cache.models.erase(&fixture);
    // Loaded client models, including custom models: compare the shared category/mask to the legacy reference.
    const auto checkModel = [&](const qmodel_t* model) {
        if(!model) { return; }
        ++models;
        rightBones(model, right); makeRightBones(model, rightExpected);
        require(!memcmp(right, rightExpected, sizeof(right)), "loaded right masks");
        require(category(model) == classifyModel(model), "loaded category");
        require(bodyParts(model, parts) == makeBodyParts(model, expected) &&
            !memcmp(parts, expected, sizeof(parts)), "loaded body masks");
    };
    for(int i = 0; i < MAX_MODELS; i++) { checkModel(cl.model_precache[i]); }
    Con_Printf("modelmetadata: PASS checks=%d loaded=%d\n", checks, models);
}
} // namespace qvr::modelmeta

extern "C" void VR_ModelMetadataChanged(const qmodel_t* model)
{
    qvr::modelmeta::changed(model);
}

extern "C" enum vr_model_id VR_ModelIdentity(const qmodel_t* model)
{
    return static_cast<vr_model_id>(qvr::modelmeta::get(model).id);
}
extern "C" int VR_ModelHasTrait(const qmodel_t* model, enum vr_model_trait trait)
{
    return trait >= 0 && trait < VR_MODEL_TRAIT_Count &&
        qvr::modelmeta::get(model).has(static_cast<qvr::modelmeta::Trait>(trait));
}
