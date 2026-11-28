#pragma once
#include <map>
#include "onx_panel_layout.h"
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "onx_entity_config.h"
#include "onx_presets.h"
#include "onx_render.h"
#include "onx_navigation.h"
#include "onx_panel_ui.h"
#include "onx_panel_detail.h"

namespace onx_panel_pages {
using onx_panel_layout::Page;
using onx_panel_layout::Kind;
// Pages use stable configuration indices so all keys share HA subscriptions.
constexpr size_t MAX_PAGES=onx_memory::ENTRY_COUNT;
constexpr uint8_t NO_ENTRY=0xFF;
using onx_panel_layout::MAX_CELLS;
// The artwork index uses seven bits. Keep the presentation flag in its
// unused high bit so existing page preferences retain their exact format.
constexpr uint8_t ICON_ONLY=0x80;
constexpr uint8_t ART_INDEX=0x7F;
static_assert(onx_panel_artwork::LIMIT<=ART_INDEX);
struct StoredPage {
  char name[49]{}, icon[65]{};
  uint8_t kind{0}, rows{2}, cols{2}, carousel_artwork{0};
  std::array<uint8_t,MAX_CELLS> entries{}, artwork{};
  StoredPage() { entries.fill(NO_ENTRY); }
};
struct StoredPages {
  static constexpr uint32_t MAGIC=0x52435034;
  uint32_t magic{MAGIC};
  uint16_t count{0};
  StoredPage pages[MAX_PAGES]{};
  std::array<uint64_t,onx_panel_artwork::LIMIT> images{};
};
inline StoredPages &stored=onx_memory::psram_object<StoredPages>();
// Icon animations for action-grid keys, per entity entry (so they follow the
// entity through page edits). A separate preference keeps StoredPages' format.
enum Effect : uint8_t {EFFECT_NONE=0,EFFECT_PULSE=1,EFFECT_BLINK=2,EFFECT_SPIN=3};
inline const char *effect_name(uint8_t e){return e==EFFECT_PULSE?"pulse":e==EFFECT_BLINK?"blink":e==EFFECT_SPIN?"spin":"none";}
inline int effect_parse(const std::string &s){return s=="none"?EFFECT_NONE:s=="pulse"?EFFECT_PULSE:s=="blink"?EFFECT_BLINK:s=="spin"?EFFECT_SPIN:-1;}
struct StoredEffects {
  static constexpr uint32_t MAGIC=0x52434659;
  uint32_t magic{MAGIC};
  std::array<uint8_t,onx_memory::ENTRY_COUNT> fx{};
};
inline StoredEffects &effects=onx_memory::psram_object<StoredEffects>();
inline uint8_t effect_of(uint8_t entry){return entry<onx_memory::ENTRY_COUNT?effects.fx[entry]:EFFECT_NONE;}
inline bool loaded=false;
// Set when entries change (slot save, preset, import); reconciled lazily.
inline bool dirty=true;
inline int size_of(Kind k){return k==Kind::TWO_CONTROLS?2:k==Kind::CONTROL_SENSORS?4:1;}
inline uint16_t stack_code(const StoredPage &page){return uint16_t(page.rows|(page.cols<<8));}
inline int size_of(const StoredPage &page) {
  const Kind k=Kind(page.kind);
  return k==Kind::ACTION_GRID ? page.rows*page.cols : k==Kind::STACK ? onx_panel_layout::stack_cells(stack_code(page)) : size_of(k);
}
// Positions with an entity (stack chip strips spread over the filled ones).
inline uint32_t filled_mask(const StoredPage &page) {
  uint32_t m=0;for(int i=0;i<size_of(page);++i)if(page.entries[i]!=NO_ENTRY)m|=1u<<i;return m;
}
inline std::array<onx_panel_layout::Rect,MAX_CELLS> page_geometry(const StoredPage *p,int w,int h,bool detail) {
  if(p && Kind(p->kind)==Kind::STACK)return onx_panel_layout::stack_geometry(stack_code(*p),w,h,detail,filled_mask(*p));
  return onx_panel_layout::geometry(p?Kind(p->kind):Kind::SINGLE,w,h,detail,p?p->rows:2,p?p->cols:2);
}
inline uint8_t lead(const StoredPage &page) {
  for(int i=0;i<size_of(page);++i)if(page.entries[i]!=NO_ENTRY)return page.entries[i];
  return NO_ENTRY;
}
inline int configured(const std::string &entity) {
  if(entity.empty())return -1;
  const size_t limit=custom_logic::active_slot_limit();
  for(size_t i=0;i<limit;++i)if(custom_logic::cached_device_entity_id(i)==entity)return int(i);
  return -1;
}
inline std::string entry_entity(uint8_t entry) {
  return entry<onx_memory::ENTRY_COUNT?custom_logic::cached_device_entity_id(entry):std::string();
}
inline constexpr char ENTITY_ICON[]="entity";
// "entity:<entity_id>" in the API uses that entity's live icon; it is stored
// as "entity:#<entry>" (the entry index is stable and fits the icon field).
inline constexpr char ENTITY_REF[]="entity:";
inline bool entity_icon(const char *icon) {
  return std::strcmp(icon,ENTITY_ICON)==0 || std::strncmp(icon,ENTITY_REF,7)==0;
}
// The entity whose icon the page shows, or "" for a fixed/default icon.
inline std::string icon_entity(const StoredPage &page) {
  if(std::strcmp(page.icon,ENTITY_ICON)==0)return entry_entity(lead(page));
  if(std::strncmp(page.icon,"entity:#",8)!=0)return "";
  const int entry=std::atoi(page.icon+8);
  return entry>=0 && entry<int(onx_memory::ENTRY_COUNT)?entry_entity(uint8_t(entry)):std::string();
}
inline bool placed(const StoredPages &list,int entry) {
  for(int p=0;p<list.count;++p)for(auto e:list.pages[p].entries)if(e==entry)return true;
  return false;
}
// Compact variable-length NVS persistence in namespace "rc_pages" so 10 pages
// take ~320 bytes and 100+ pages stay well under NVS blob limits.
inline bool save_pages_compact(const StoredPages &next, const StoredEffects &next_effects) {
#ifdef USE_ESP32
  std::vector<uint8_t, esphome::RAMAllocator<uint8_t>> blob;
  blob.reserve(64 + size_t(next.count) * 48);
  auto put = [&](const void *p, size_t n) {
    const auto *b = static_cast<const uint8_t *>(p);
    blob.insert(blob.end(), b, b + n);
  };
  uint32_t magic = StoredPages::MAGIC;
  uint16_t count = std::min<uint16_t>(next.count, MAX_PAGES);
  uint8_t img_count = 0;
  for (size_t i = 0; i < next.images.size(); ++i) if (next.images[i]) img_count = uint8_t(i + 1);
  put(&magic, 4); put(&count, 2); blob.push_back(img_count);
  for (uint8_t i = 0; i < img_count; ++i) put(&next.images[i], 8);
  for (uint16_t p = 0; p < count; ++p) {
    const auto &pg = next.pages[p];
    const uint8_t nlen = uint8_t(strnlen(pg.name, 48));
    const uint8_t ilen = uint8_t(strnlen(pg.icon, 64));
    const uint8_t cells = uint8_t(std::clamp(size_of(pg), 1, MAX_CELLS));
    blob.push_back(pg.kind); blob.push_back(pg.rows); blob.push_back(pg.cols); blob.push_back(pg.carousel_artwork);
    blob.push_back(nlen); if (nlen) put(pg.name, nlen);
    blob.push_back(ilen); if (ilen) put(pg.icon, ilen);
    blob.push_back(cells);
    put(pg.entries.data(), cells);
    put(pg.artwork.data(), cells);
  }
  std::vector<uint8_t, esphome::RAMAllocator<uint8_t>> fx_blob;
  uint32_t fx_magic = StoredEffects::MAGIC;
  uint16_t fx_count = 0;
  for (size_t i = 0; i < onx_memory::ENTRY_COUNT; ++i) if (next_effects.fx[i]) ++fx_count;
  fx_blob.reserve(6 + fx_count * 2);
  const auto *mp = reinterpret_cast<const uint8_t *>(&fx_magic);
  fx_blob.insert(fx_blob.end(), mp, mp + 4);
  const auto *cp = reinterpret_cast<const uint8_t *>(&fx_count);
  fx_blob.insert(fx_blob.end(), cp, cp + 2);
  for (size_t i = 0; i < onx_memory::ENTRY_COUNT; ++i) {
    if (!next_effects.fx[i]) continue;
    fx_blob.push_back(uint8_t(i));
    fx_blob.push_back(next_effects.fx[i]);
  }
  nvs_handle_t handle;
  if (nvs_open("rc_pages", NVS_READWRITE, &handle) != ESP_OK) return false;
  esp_err_t err = nvs_set_blob(handle, "pages_v4", blob.data(), blob.size());
  if (err == ESP_OK) err = nvs_set_blob(handle, "fx_v2", fx_blob.data(), fx_blob.size());
  if (err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  return err == ESP_OK;
#else
  (void)next; (void)next_effects;
  return true;
#endif
}
inline bool load_pages_compact(StoredPages &out, StoredEffects &out_fx) {
#ifdef USE_ESP32
  nvs_handle_t handle;
  if (nvs_open("rc_pages", NVS_READONLY, &handle) != ESP_OK) return false;
  size_t len = 0;
  bool ok = nvs_get_blob(handle, "pages_v4", nullptr, &len) == ESP_OK && len >= 7;
  std::vector<uint8_t, esphome::RAMAllocator<uint8_t>> blob;
  if (ok) {
    blob.resize(len);
    ok = blob.size() == len && nvs_get_blob(handle, "pages_v4", blob.data(), &len) == ESP_OK;
  }
  size_t fx_len = 0;
  std::vector<uint8_t, esphome::RAMAllocator<uint8_t>> fx_blob;
  if (nvs_get_blob(handle, "fx_v2", nullptr, &fx_len) == ESP_OK && fx_len >= 6) {
    fx_blob.resize(fx_len);
    if (fx_blob.size() == fx_len && nvs_get_blob(handle, "fx_v2", fx_blob.data(), &fx_len) == ESP_OK) {
      uint32_t fmagic = 0; uint16_t fcount = 0;
      std::memcpy(&fmagic, fx_blob.data(), 4); std::memcpy(&fcount, fx_blob.data() + 4, 2);
      if (fmagic == StoredEffects::MAGIC && fx_blob.size() == 6 + size_t(fcount) * 2) {
        out_fx = StoredEffects{};
        for (uint16_t i = 0; i < fcount; ++i) {
          const uint8_t entry = fx_blob[6 + i * 2], val = fx_blob[6 + i * 2 + 1];
          if (entry < onx_memory::ENTRY_COUNT && val <= EFFECT_SPIN) out_fx.fx[entry] = val;
        }
      }
    }
  }
  nvs_close(handle);
  if (!ok) return false;
  uint32_t magic = 0; uint16_t count = 0;
  std::memcpy(&magic, blob.data(), 4); std::memcpy(&count, blob.data() + 4, 2);
  const uint8_t img_count = blob[6];
  if (magic != StoredPages::MAGIC || count > MAX_PAGES || img_count > out.images.size()) return false;
  size_t pos = 7;
  if (pos + size_t(img_count) * 8 > blob.size()) return false;
  out.magic = StoredPages::MAGIC;
  out.count = 0;
  out.images.fill(0);
  for (uint8_t i = 0; i < img_count; ++i) { std::memcpy(&out.images[i], blob.data() + pos, 8); pos += 8; }
  for (uint16_t p = 0; p < count; ++p) {
    if (pos + 6 > blob.size()) return false;
    StoredPage pg{};
    pg.kind = blob[pos++]; pg.rows = blob[pos++]; pg.cols = blob[pos++]; pg.carousel_artwork = blob[pos++];
    const uint8_t nlen = blob[pos++];
    if (nlen > 48 || pos + nlen + 1 > blob.size()) return false;
    if (nlen) std::memcpy(pg.name, blob.data() + pos, nlen);
    pg.name[nlen] = 0; pos += nlen;
    const uint8_t ilen = blob[pos++];
    if (ilen > 64 || pos + ilen + 1 > blob.size()) return false;
    if (ilen) std::memcpy(pg.icon, blob.data() + pos, ilen);
    pg.icon[ilen] = 0; pos += ilen;
    const uint8_t cells = blob[pos++];
    if (cells > MAX_CELLS || pos + size_t(cells) * 2 > blob.size()) return false;
    if (cells) {
      std::memcpy(pg.entries.data(), blob.data() + pos, cells); pos += cells;
      std::memcpy(pg.artwork.data(), blob.data() + pos, cells); pos += cells;
    }
    out.pages[out.count++] = pg;
  }
  return pos == blob.size();
#else
  (void)out; (void)out_fx;
  return false;
#endif
}
// Drops entries that were cleared and gives every unplaced entry its own page,
// so nothing configured becomes unreachable.
inline void reconcile(StoredPages &list) {
  int out=0;
  for(int p=0;p<list.count;++p) {
    auto page=list.pages[p];
    // Grids and stacks keep their gaps; fixed layouts fall apart into singles.
    const bool grid=Kind(page.kind)==Kind::ACTION_GRID || Kind(page.kind)==Kind::STACK;
    const int n=size_of(page);
    bool complete=true;
    for(int i=0;i<n;++i) {
      if(!entry_entity(page.entries[i]).empty())continue;
      page.entries[i]=NO_ENTRY;
      page.artwork[i]=0;
      complete=false;
    }
    if(grid && lead(page)==NO_ENTRY)continue;
    if(!grid && !complete) {
      // Keep the remaining entries from an incomplete legacy layout.
      for(int i=0;i<n;++i)if(page.entries[i]!=NO_ENTRY && out<int(MAX_PAGES)) {
        StoredPage single{};
        single.artwork[0]=page.artwork[i];
        if(i==0)single.carousel_artwork=page.carousel_artwork;
        if(i==0){std::memcpy(single.name,page.name,sizeof(single.name));std::memcpy(single.icon,page.icon,sizeof(single.icon));}
        single.entries[0]=page.entries[i];list.pages[out++]=single;
      }
      continue;
    }
    if(out>=int(MAX_PAGES))break;
    for(int i=n;i<MAX_CELLS;++i)page.entries[i]=NO_ENTRY;
    // If clearing an entry changed the lead to one already used, leave
    // the remaining entities for the unplaced-entry pass.
    const auto first=lead(page);
    if(std::any_of(list.pages,list.pages+out,[first](const StoredPage &p){return lead(p)==first;}))continue;
    list.pages[out++]=page;
  }
  list.count=out;
  // Registered timers (timer./sensor. with the timer flag) need no page: their
  // popups and the timer screen find them without a carousel stop.
  const size_t limit = custom_logic::active_slot_limit();
  for(size_t e=0;e<limit && list.count<MAX_PAGES;++e)
    if(!entry_entity(uint8_t(e)).empty() && !placed(list,int(e)) && !onx_entity_config::timer_hidden(e,entry_entity(uint8_t(e)))){StoredPage single{};single.entries[0]=uint8_t(e);list.pages[list.count++]=single;}
}
// No saved layout: one page per configured entity.
inline void default_pages() {
  stored.magic=StoredPages::MAGIC;
  stored.count=0;
  stored.images.fill(0);
  const size_t limit = custom_logic::active_slot_limit();
  for(size_t e=0;e<limit;++e) {
    const auto entity=entry_entity(uint8_t(e));if(entity.empty() || placed(stored,int(e)) || onx_entity_config::timer_hidden(e,entity))continue;
    StoredPage page{};page.entries[0]=uint8_t(e);
    stored.pages[stored.count++]=page;
  }
}
inline bool load() {
  if(loaded)return true;
  if(!esphome::global_preferences)return false;
  effects = StoredEffects{};
  if(!load_pages_compact(stored, effects)) default_pages();
  for(int i=0;i<stored.count;++i) {
    auto &p=stored.pages[i];
    p.name[48]=0;p.icon[64]=0;
    const bool ok=p.kind==uint8_t(Kind::STACK) ? onx_panel_layout::valid_stack(stack_code(p))
                 : p.kind<=uint8_t(Kind::ACTION_GRID) && onx_panel_layout::grid_axis(p.rows) && onx_panel_layout::grid_axis(p.cols);
    if(!ok) {
      p.kind=0;p.rows=2;p.cols=2;
    }
  }
  loaded=true;return true;
}
inline uint32_t seen_revision=0;
inline void refresh(){if(dirty || seen_revision!=custom_logic::entity_revision){reconcile(stored);dirty=false;seen_revision=custom_logic::entity_revision;}}
inline int page_index(const std::string &entity) {
  if(!load())return -1;refresh();
  const int entry=configured(entity);if(entry<0)return -1;
  // An entity may appear on several pages (one entry, one subscription); the
  // page it leads wins.
  for(int p=0;p<stored.count;++p)if(lead(stored.pages[p])==entry)return p;
  for(int p=0;p<stored.count;++p)for(auto e:stored.pages[p].entries)if(e==entry)return p;
  return -1;
}
// Page name/icon for the carousel; empty when the page uses its entity's.
inline const StoredPage *info(const std::string &entity) {
  const int p=page_index(entity);return p<0?nullptr:&stored.pages[p];
}
inline const StoredPage *led_by(const std::string &entity) {
  const auto *p=info(entity);
  return p && entry_entity(lead(*p))==entity?p:nullptr;
}
inline std::vector<int> order() {
  std::vector<int> out;
  if(!load()) {
    for(int e=0;e<int(onx_memory::ENTRY_COUNT);++e)if(!entry_entity(e).empty())out.push_back(int(custom_logic::normal_ui_slot_for_device_config_slot(e)));
    return out;
  }
  refresh();
  for(int p=0;p<stored.count;++p)out.push_back(int(custom_logic::normal_ui_slot_for_device_config_slot(lead(stored.pages[p]))));
  return out;
}
inline const bool order_installed=(onx_entity_config::page_order=&order,true);
inline std::string rpc(const std::string &op,const std::string &data,const std::string &asset={},unsigned offset=0,const std::string &total_str={});
inline uint64_t artwork_id(const StoredPages &list,const StoredPage &p,int pos) {
  const auto index=pos<0?p.carousel_artwork:p.artwork[pos]&ART_INDEX;
  return index && index<=list.images.size()?list.images[index-1]:0;
}
inline bool set_artwork(StoredPages &list,StoredPage &p,int pos,uint64_t value) {
  for(size_t i=0;i<list.images.size();++i) {
    if(list.images[i] && list.images[i]!=value)continue;
    list.images[i]=value;(pos<0?p.carousel_artwork:p.artwork[pos])=i+1;return true;
  }
  return false;
}
inline bool bind_carousel_artwork(lv_obj_t *icon,const StoredPage &page) {
  const auto value=artwork_id(stored,page,-1);
  if(!value)return false;
  const auto slot=std::find(onx_navigation::icons.begin(),onx_navigation::icons.end(),icon);
  if(slot==onx_navigation::icons.end())return false;
  static onx_memory::StateArray<onx_panel_artwork::Image,onx_navigation::VISIBLE_ICONS> cache;
  auto &art=cache[slot-onx_navigation::icons.begin()];
  if(!art.load(value))return false;
  lv_image_set_src(icon,art.source);
  lv_image_set_scale(icon,onx_navigation::icon_size()*256/art.source->header.w);
  lv_obj_set_style_image_recolor_opa(icon,LV_OPA_TRANSP,0);
  return true;
}
inline bool resolve_artwork(StoredPages &list,StoredPage &page,int pos,const std::string &ref) {
  if(ref.empty())return true;
  const auto value=onx_panel_artwork::id(ref);
  return value && onx_panel_artwork::read(value,nullptr) && set_artwork(list,page,pos,value);
}
inline void encode(JsonArray out) {
  if(!load())return;refresh();
  for(int p=0;p<stored.count;++p) {
    const auto &page=stored.pages[p];auto item=out.add<JsonObject>();
    item["name"]=std::string(page.name);item["icon"]=std::strncmp(page.icon,"entity:#",8)==0?std::string(ENTITY_REF)+icon_entity(page):std::string(page.icon);
    item["layout"]=onx_panel_layout::name(Kind(page.kind));
    if(page.carousel_artwork)item["carousel_artwork"]=onx_panel_artwork::key(artwork_id(stored,page,-1));
    if(Kind(page.kind)==Kind::ACTION_GRID){item["rows"]=page.rows;item["columns"]=page.cols;}
    if(Kind(page.kind)==Kind::STACK) {
      auto rows=item["stack"].to<JsonArray>();const auto code=stack_code(page);
      for(int r=0;r<onx_panel_layout::stack_rows(code);++r)rows.add(onx_panel_layout::row_name(onx_panel_layout::stack_row(code,r)));
    }
    auto entities=item["entities"].to<JsonArray>();
    for(int i=0;i<size_of(page);++i)entities.add(entry_entity(page.entries[i]));
    if(std::any_of(page.artwork.begin(),page.artwork.begin()+size_of(page),[](uint8_t i){return (i&ART_INDEX)!=0;})) {
      auto images=item["artwork"].to<JsonArray>();
      for(int i=0;i<size_of(page);++i)images.add(onx_panel_artwork::key(artwork_id(stored,page,i)));
    }
    if(Kind(page.kind)==Kind::ACTION_GRID &&
       std::any_of(page.artwork.begin(),page.artwork.end(),[](uint8_t i){return (i&ICON_ONLY)!=0;})) {
      auto flags=item["icon_only"].to<JsonArray>();
      for(int i=0;i<size_of(page);++i)flags.add((page.artwork[i]&ICON_ONLY)!=0);
    }
    if(Kind(page.kind)==Kind::ACTION_GRID) {
      JsonObject animation;
      for(int i=0;i<size_of(page);++i) {
        const auto fx=effect_of(page.entries[i]);if(!fx)continue;
        if(animation.isNull())animation=item["animation"].to<JsonObject>();
        animation[entry_entity(page.entries[i])]=effect_name(fx);
      }
    }
  }
}
// A preset's pages, or {"pages":[]} for one page per entity. Invalid lists
// (e.g. an entry failed to save) fall back to one page per entity.
inline void apply(const std::string &json) {
  dirty=true;
  const auto result=rpc("panel_pages_save",json.empty()?std::string("{\"pages\":[]}"):json);
  if(result.find("\"error\"")!=std::string::npos)rpc("panel_pages_save","{\"pages\":[]}");
}
inline const bool presets_installed=(onx_presets::encode_pages=&encode,onx_presets::apply_pages=&apply,true);
inline bool valid_icon(const std::string &icon) {
  if(icon.empty() || icon==ENTITY_ICON)return true;
  if(icon.rfind(ENTITY_REF,0)==0)return onx_config::valid_entity(icon.substr(7));
  if(icon.size()>64 || icon.rfind("mdi:",0)!=0 || icon.size()<5)return false;
  for(size_t i=4;i<icon.size();++i){const char c=icon[i];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'))return false;}
  return true;
}
// Reclaim images that no saved panel page or preset uses. Recent uploads may
// still be in a browser's save transaction.
inline std::array<std::pair<uint64_t,uint32_t>,onx_panel_artwork::LIMIT> recently_uploaded{};
inline void prune_artwork() {
  std::vector<uint64_t> keep;
  auto add=[&keep](uint64_t value){if(value && std::find(keep.begin(),keep.end(),value)==keep.end())keep.push_back(value);};
  if(!load())return;
  for(int i=0;i<stored.count;++i)for(int j=-1;j<size_of(stored.pages[i]);++j)add(artwork_id(stored,stored.pages[i],j));
  const uint32_t now=esphome::millis();
  for(const auto &[value,when]:recently_uploaded)if(now-when<120000)add(value);
  onx_presets::load();
  for(const auto &preset:onx_presets::summaries) {
    onx_presets::Blob raw;
    if(!onx_presets::store_read(onx_presets::key_name(preset.key),raw))return;
    auto doc=custom_logic::make_psram_json_document();
    if(deserializeJson(doc,raw.c_str(),raw.size()))return;
    for(JsonObjectConst page:doc["pages"].as<JsonArrayConst>()) {
      if(const char *key=page["carousel_artwork"].as<const char *>())add(onx_panel_artwork::id(key));
      for(const char *key:page["artwork"].as<JsonArrayConst>())if(key)add(onx_panel_artwork::id(key));
    }
  }
  onx_panel_artwork::remove_unused([&keep](uint64_t value){return std::find(keep.begin(),keep.end(),value)!=keep.end();});
}
inline std::string artwork_rpc(const std::string &op,const std::string &data,const std::string &asset,unsigned offset) {
  using namespace onx_panel_artwork;
  using Buffer=std::array<uint8_t,BYTES>;
  if(op=="panel_art_get") {
    auto pixels=custom_logic::make_ram_allocated<Buffer>();
    if(!pixels)return R"({"error":"Could not allocate image storage"})";
    const size_t size=read(id(data),pixels->data(),pixels->size());
    if(offset>=size || offset%CHUNK)return R"({"error":"Image is unavailable"})";
    const size_t length=std::min(CHUNK,size-offset);
    custom_logic::PsramString encoded(encoded_size(length)+1,'\0');
    size_t written=0;
    if(mbedtls_base64_encode(reinterpret_cast<uint8_t *>(encoded.data()),encoded.size(),&written,pixels->data()+offset,length)!=0)
      return R"({"error":"Could not encode image"})";
    encoded.resize(written);
    return onx_entity_config::build_json([&](JsonObject root){
      root["data"]=encoded.c_str();root["next"]=offset+length;root["total"]=size;
    });
  }
  struct Upload {
    custom_logic::RamAllocatedPtr<Buffer> pixels;
    uint64_t id{0};
    size_t next{0};
    uint32_t touched{0};
  };
  static Upload upload;
  const auto expected=id(asset);
  const bool legacy=asset.empty();
  if((!legacy && !expected) || (legacy && offset) || offset>=BYTES || offset%CHUNK)
    return R"({"error":"Invalid image upload offset"})";
  const size_t chunk=legacy?LEGACY_BYTES:std::min(CHUNK,BYTES-offset);
  if(data.size()!=encoded_size(chunk))return R"({"error":"Invalid square image"})";
  const uint32_t received=esphome::millis();
  if(!offset) {
    upload.pixels=custom_logic::make_ram_allocated<Buffer>();
    upload.id=expected;upload.next=0;upload.touched=received;
  }
  if(!upload.pixels || upload.id!=expected || upload.next!=offset || received-upload.touched>30000)
    return R"({"error":"Image upload expired. Save the image again."})";
  upload.touched=received;
  custom_logic::PsramString encoded(data.begin(),data.end());
  std::replace(encoded.begin(),encoded.end(),'-','+');
  std::replace(encoded.begin(),encoded.end(),'_','/');
  size_t written=0;
  auto &pixels=*upload.pixels;
  if(mbedtls_base64_decode(pixels.data()+offset,chunk,&written,reinterpret_cast<const uint8_t *>(encoded.data()),encoded.size())!=0 || written!=chunk)
    return R"({"error":"Invalid square image"})";
  upload.next+=written;
  const size_t size=legacy?LEGACY_BYTES:BYTES;
  if(upload.next<size)return "{\"next\":"+std::to_string(upload.next)+"}";
  auto complete=std::move(upload.pixels);
  const auto value=hash(complete->data(),size);
  if(expected && expected!=value)return R"({"error":"Image checksum differs from the selected crop"})";
  auto existing=custom_logic::make_ram_allocated<Buffer>();
  if(!existing)return R"({"error":"Could not allocate image storage"})";
  const size_t existing_size=read(value,existing->data(),existing->size());
  if(existing_size) {
    if(existing_size!=size || !std::equal(complete->begin(),complete->begin()+size,existing->begin()))
      return R"({"error":"Image conflicts with one already stored"})";
  } else {
    if(count()>=LIMIT || !write(value,complete->data(),size)) {
      prune_artwork();
      if(count()>=LIMIT || !write(value,complete->data(),size))
        return R"({"error":"Panel image storage is full"})";
    }
  }
  const auto now=esphome::millis();
  auto oldest=recently_uploaded.begin();
  for(auto it=recently_uploaded.begin();it!=recently_uploaded.end();++it) {
    if(it->first==value || !it->first){oldest=it;break;}
    if(now-it->second>now-oldest->second)oldest=it;
  }
  *oldest={value,now};
  return "{\"id\":\""+key(value)+"\"}";
}
inline std::string rpc(const std::string &op,const std::string &data,const std::string &asset,unsigned offset,const std::string &total_str) {
  if(op=="panel_art_get" || op=="panel_art_put")return artwork_rpc(op,data,asset,offset);
  // Native (portrait) size and the orientation in use since boot, so the
  // configuration preview matches either model and orientation.
  if(op=="panel_screen")return onx_entity_config::build_json([](JsonObject root) {
    root["width"]=onx_panel_ui::native_width;root["height"]=onx_panel_ui::native_height;
    root["orientation"]=onx_panel_ui::turns*90;
  });
  if(op=="panel_pages_put") {
    // Chunked page list save (for large page/grid configurations > 4 KB).
    constexpr size_t MAX_PAGES_JSON = 65536;
    static custom_logic::PsramString upload_buf;
    static size_t upload_total = 0;
    static uint32_t upload_touched = 0;
    unsigned total = 0;
    if (total_str.empty()) return R"({"error":"Invalid page upload size"})";
    for (char c : total_str) {
      if (c < '0' || c > '9') return R"({"error":"Invalid page upload size"})";
      total = total * 10 + unsigned(c - '0');
    }
    const uint32_t now = esphome::millis();
    if (offset == 0) {
      upload_buf.clear();
      if (total == 0 || total > MAX_PAGES_JSON) return R"({"error":"Page configuration is too large"})";
      upload_total = total;
      upload_touched = now;
      upload_buf.reserve(total);
    } else if (offset != upload_buf.size() || total != upload_total || now - upload_touched > 30000) {
      upload_buf.clear();
      return R"({"error":"Page upload expired; save again"})";
    }
    upload_touched = now;
    if (upload_buf.size() + data.size() > upload_total) {
      upload_buf.clear();
      return R"({"error":"Page upload exceeds announced size"})";
    }
    upload_buf.append(data.data(), data.size());
    if (upload_buf.size() < upload_total) return "{\"next\":" + std::to_string(upload_buf.size()) + "}";
    std::string assembled(upload_buf.data(), upload_buf.size());
    custom_logic::PsramString().swap(upload_buf);
    upload_total = 0;
    return rpc("panel_pages_save", assembled);
  }
  if(op!="panel_pages" && op!="panel_pages_save")return "";
  if(!load())return "{\"error\":\"Page storage is not ready\"}";
  if(op=="panel_pages_save") {
    auto doc = custom_logic::make_psram_json_document();
    if(deserializeJson(doc,data)!=DeserializationError::Ok)return "{\"error\":\"Invalid page configuration\"}";
    auto pages=doc["pages"].as<JsonArrayConst>();
    if(pages.size()>MAX_PAGES)return "{\"error\":\"Too many pages\"}";
    auto next_buffer=custom_logic::make_ram_allocated<StoredPages>();
    auto next_fx_buffer=custom_logic::make_ram_allocated<StoredEffects>();
    if(!next_buffer || !next_fx_buffer)return "{\"error\":\"Not enough memory to save pages\"}";
    auto &next=*next_buffer;
    auto &next_effects=*next_fx_buffer;
    for(JsonObjectConst item:pages) {
      StoredPage page{};Kind kind;
      const std::string name=item["name"] | "";std::string icon=item["icon"] | "";
      if(!onx_panel_layout::parse(item["layout"] | "",kind))return "{\"error\":\"Choose a layout for every page\"}";
      if(name.size()>48)return "{\"error\":\"Page names are limited to 48 bytes\"}";
      if(!valid_icon(icon))return "{\"error\":\"Icons use the mdi:name form\"}";
      if(icon.rfind(ENTITY_REF,0)==0) {
        const int entry=configured(icon.substr(7));
        if(entry<0)return R"({"error":"Save the icon's entity before the pages"})";
        icon="entity:#"+std::to_string(entry);
      }
      page.kind=uint8_t(kind);
      if(kind==Kind::ACTION_GRID) {
        const int rows=item["rows"] | 0, cols=item["columns"] | 0;
        if(!onx_panel_layout::grid_axis(rows) || !onx_panel_layout::grid_axis(cols))
          return R"({"error":"Choose 1–4 rows and columns"})";
        page.rows=rows;page.cols=cols;
      }
      if(kind==Kind::STACK) {
        auto rows=item["stack"].as<JsonArrayConst>();
        if(rows.isNull() || rows.size()<1 || rows.size()>size_t(onx_panel_layout::MAX_ROWS))return R"({"error":"A stack has 1–6 rows"})";
        uint32_t code=0,m=1;
        for(JsonVariantConst r:rows) {
          onx_panel_layout::Row row;
          if(!onx_panel_layout::parse_row(r | "",row))return R"({"error":"Stack rows are full, split, compact or chips"})";
          code+=uint32_t(row)*m;m*=5;
        }
        if(!onx_panel_layout::valid_stack(uint16_t(code)))return R"({"error":"A stack holds at most 16 entities"})";
        page.rows=uint8_t(code&0xFF);page.cols=uint8_t(code>>8);
      }
      if(!resolve_artwork(next,page,-1,item["carousel_artwork"] | ""))
        return R"({"error":"Carousel image is unavailable. Re-upload it before saving."})";
      auto entities=item["entities"].as<JsonArrayConst>();
      auto images=item["artwork"].as<JsonArrayConst>();
      auto flags=item["icon_only"].as<JsonArrayConst>();
      if(!item["icon_only"].isNull() && (kind!=Kind::ACTION_GRID || flags.size()!=entities.size()))
        return R"({"error":"Icon-only settings must match the action grid"})";
      if(!images.isNull() && images.size()!=entities.size())return R"({"error":"Image positions must match the layout"})";
      if(entities.size()!=size_t(size_of(page)))return R"({"error":"The number of positions must match the layout"})";
      for(int i=0;i<size_of(page);++i) {
        const std::string entity=entities[i] | "";const int entry=configured(entity);
        if(entity.empty() && (kind==Kind::ACTION_GRID || kind==Kind::STACK))continue;
        if(entry<0)return "{\"error\":\"Save the page entities before the pages\"}";
        // Reuse is fine (same entry, same subscription), but a carousel stop
        // needs one leading entity and a page shows an entity once.
        for(int j=0;j<i;++j)if(page.entries[j]==entry)return "{\"error\":\"A page shows each entity once\"}";
        if(kind==Kind::CONTROL_SENSORS && i>0 && !onx_panel_layout::sensor(entity))return "{\"error\":\"Compact positions show sensors and binary sensors\"}";
        page.entries[i]=uint8_t(entry);
        if(!resolve_artwork(next,page,i,images[i] | ""))
          return R"({"error":"Page image is unavailable. Re-upload it before saving."})";
        if(flags[i] | false)page.artwork[i]|=ICON_ONLY;
      }
      const auto first=lead(page);
      if(first==NO_ENTRY)return kind==Kind::STACK ? R"({"error":"Place at least one entity on each stack"})" : R"({"error":"Place at least one action on each grid"})";
      if(!item["animation"].isNull()) {
        if(kind!=Kind::ACTION_GRID || !item["animation"].is<JsonObjectConst>())return R"({"error":"Icon animations are set on action grids"})";
        for(JsonPairConst fx:item["animation"].as<JsonObjectConst>()) {
          const int entry=configured(fx.key().c_str()),value=effect_parse(fx.value() | "");
          if(value<0)return R"({"error":"Choose none, pulse, blink or spin"})";
          if(entry>=0 && entry<int(onx_memory::ENTRY_COUNT))next_effects.fx[entry]=uint8_t(value);
        }
      }
      for(int p=0;p<next.count;++p)
        if(lead(next.pages[p])==first)return R"({"error":"An entity can lead only one page"})";
      snprintf(page.name,sizeof(page.name),"%s",name.c_str());snprintf(page.icon,sizeof(page.icon),"%s",icon.c_str());
      next.pages[next.count++]=page;
    }
    reconcile(next);
    if(!save_pages_compact(next, next_effects))
      return "{\"error\":\"Could not save pages\"}";
    stored=next;effects=next_effects;
    onx_grid_cache::invalidate_all_pages();
  }
  reconcile(stored);
  return onx_entity_config::build_json([](JsonObject root) {
    encode(root["pages"].to<JsonArray>());
  });
}
inline onx_detail_view::View timer_view(const std::string &entity,const std::string &title) {
  auto result=onx_detail_view::build(entity,title);
  const auto *d=custom_logic_entity_sub::entity_detail(entity);
  if(!d)return result;
  onx_timer::View timer;
  if(d->voice_timer) {
    const uint32_t age=esphome::millis()-d->timer_received_ms;
    if(d->state!="active" || age<5000)
      timer=onx_timer::view(d->state,d->voice_duration,d->voice_remaining,d->voice_remaining+1,1.0+age/1000.0);
  }else{
    time_t finish=0;
    const bool known=custom_logic_entity_sub::parse_ha_datetime_epoch(d->timer_finishes_at,&finish) ||
        (d->state=="idle" && custom_logic_entity_sub::parse_ha_datetime_epoch(d->timer_last_finish,&finish));
    timer=onx_timer::view(d->state,onx_timer::duration_seconds(d->timer_duration),
        onx_timer::duration_seconds(d->timer_remaining),onx_timer::deadline_epoch(known,finish),std::time(nullptr));
  }
  timer.ready &= d->available && d->received;
  result.value.text=timer.ready?onx_timer::countdown(timer.seconds_left):"--:--";
  result.context.text=!timer.ready?"Syncing":d->state=="paused"?"Paused":d->state=="idle"?"Idle":
      d->state=="ringing" || timer.seconds_left<=0?"Time is up":"Running";
  result.badge.text="Timer";result.value_ring=timer.ready?int(timer.fraction*1000):-1;
  return result;
}
struct Item {std::string entity,name,value,context,hint,icon; int ring{-1};bool available{false},graph{false},on{false};};
inline std::function<onx_detail_view::View(const std::string &,bool)> view_for;
inline Item item(const std::string &entity,bool detail=false) {
  Item out;out.entity=entity;
  if(configured(entity)<0){out.name="Item removed";out.value="—";out.context="Choose a replacement in page settings";return out;}
  out.name=custom_logic::ha_entity_title_from_registry_cache(entity);if(out.name.empty())out.name=entity;
  auto view=view_for?view_for(entity,detail):(detail?onx_detail_view::build(entity,out.name):onx_detail_view::carousel(entity,out.name));
  out.value=view.value.text;out.context=view.context.text;
  if(onx_panel_layout::sensor(entity) && !view.name.text.empty() && view.name.text!=out.name)out.context=view.name.text;
  if(detail && !view.badge.text.empty())out.context=view.badge.text+(out.context.empty()?"":" · "+out.context);
  out.hint=view.hint.text;
  out.icon=view.icon;out.available=view.available;out.ring=view.value_ring;out.graph=view.layout=="graph";
  // Active entities tint their icon with the accent colour.
  if(const auto *d=custom_logic_entity_sub::entity_detail(entity); d && d->available) {
    const auto &st=d->state;
    out.on=st=="on" || st=="playing" || st=="open" || st=="opening" || st=="active" ||
        st=="cleaning" || st=="returning" || st=="recording" || st=="streaming" ||
        st=="unlocked" || st=="unlocking" || st=="jammed" ||
        (entity.rfind("alarm_control_panel.",0)==0 && st!="disarmed") ||
        (entity.rfind("climate.",0)==0 && st!="off");
  }
  return out;
}
inline lv_obj_t *layer=nullptr;
inline lv_obj_t *carousel_clock=nullptr;

inline void hide_carousel_clock() {
  if (carousel_clock) lv_obj_add_flag(carousel_clock, LV_OBJ_FLAG_HIDDEN);
}

inline void refresh_carousel_clock(int width) {
  const time_t now = std::time(nullptr);
  if (now <= 1600000000) {
    if (carousel_clock) lv_obj_add_flag(carousel_clock, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  using namespace onx_panel_detail;
  const auto *font = fonts.header_clock != nullptr ? fonts.header_clock : fonts.small;
  const auto value = clock(now);
  if (!carousel_clock) {
    carousel_clock = onx_panel_detail::text(layer, value, font, TEXT(), P(16),
                                            (onx_panel_layout::MAIN_TOP - line(font)) / 2, width / 4);
  } else {
    onx_render::label(carousel_clock, value.c_str());
    onx_render::style_text_color(carousel_clock, lv_color_hex(TEXT()), 0);
    lv_obj_remove_flag(carousel_clock, LV_OBJ_FLAG_HIDDEN);
  }
}

struct Card {
  lv_obj_t *box,*name,*value,*context,*hint,*icon,*bar;
  Item previous;
  onx_panel_layout::Rect rect{};
  Kind kind{Kind::SINGLE};
  uint32_t color{0};
  bool initialized{false},detail{false};
};
inline bool same(const Item &a,const Item &b) {
  return a.entity==b.entity && a.name==b.name && a.value==b.value && a.context==b.context &&
      a.hint==b.hint && a.icon==b.icon && a.ring==b.ring && a.available==b.available && a.graph==b.graph && a.on==b.on;
}
inline std::array<Card,4> cards{};
inline std::array<onx_panel_layout::Rect,MAX_CELLS> rects{};
inline std::array<std::string,MAX_CELLS> entities{};
inline int count=0;
inline bool showing=false,detail_view=false;
inline std::string parent_entity, page_lead;
inline bool action_grid=false;
inline std::function<void(const std::string &)> run_grid_action;
inline std::function<void(const std::string &)> open;
inline std::function<void(lv_obj_t *,const std::string &,int)> draw_graph;
inline const lv_font_t *small=nullptr,*normal=nullptr,*large=nullptr;
inline void setup(const lv_font_t *s,const lv_font_t *n,const lv_font_t *l) {
  small=s;normal=n;large=l;
  layer=lv_obj_create(lv_layer_top());lv_obj_remove_flag(layer,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_set_style_bg_color(layer,lv_color_hex(onx_display_settings::background()),0);lv_obj_set_style_bg_opa(layer,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(layer,0,0);lv_obj_set_style_pad_all(layer,0,0);lv_obj_set_style_radius(layer,0,0);
  for(auto &c:cards) {
    c.box=lv_obj_create(layer);lv_obj_remove_flag(c.box,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_set_style_bg_color(c.box,lv_color_hex(0x232323),0);lv_obj_set_style_border_width(c.box,0,0);
    lv_obj_set_style_radius(c.box,8,0);lv_obj_set_style_pad_all(c.box,0,0);
    lv_obj_set_style_shadow_width(c.box,0,0);lv_obj_set_style_outline_width(c.box,0,0);
    c.name=lv_label_create(c.box);c.value=lv_label_create(c.box);c.context=lv_label_create(c.box);c.hint=lv_label_create(c.box);
    for(auto *label:{c.name,c.value,c.context,c.hint}) {
      lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);lv_obj_set_style_text_color(label,lv_color_hex(0xFFFFFF),0);
      lv_obj_remove_flag(label,LV_OBJ_FLAG_CLICKABLE);
    }
    c.icon=lv_image_create(c.box);c.bar=lv_bar_create(c.box);lv_bar_set_range(c.bar,0,1000);
    lv_obj_set_style_bg_color(c.bar,lv_color_hex(0x414850),LV_PART_MAIN);
    lv_obj_set_style_bg_color(c.bar,lv_color_hex(0x62B0FF),LV_PART_INDICATOR);
  }
  lv_obj_add_flag(layer,LV_OBJ_FLAG_HIDDEN);
}
inline std::vector<lv_obj_t *> suspended;
inline lv_obj_t *suspended_page=nullptr;
inline void restore_underlay() {
  for(auto *obj:suspended)if(lv_obj_is_valid(obj))lv_obj_remove_flag(obj,LV_OBJ_FLAG_HIDDEN);
  suspended.clear();suspended_page=nullptr;
}
inline void suspend_underlay() {
  auto *page=lv_scr_act();
  if(page!=suspended_page){restore_underlay();suspended_page=page;}
  // Keep the existing presenters/actions alive, but avoid rendering covered
  // base-layout body containers or letting their old touch widgets receive clicks.
  // The navigator and native overlays live separately on lv_layer_top().
  for(uint32_t i=0;i<lv_obj_get_child_count(page);++i) {
    auto *obj=lv_obj_get_child(page,i);
    if(!lv_obj_check_type(obj,&lv_obj_class) || lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))continue;
    if(std::find(suspended.begin(),suspended.end(),obj)==suspended.end())suspended.push_back(obj);
    lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
  }
}
// Home Assistant "Page" select: one unique title per stored page (its name,
// or its first entity's title), and the page that shows a given entry.
// An entity's name for the Page / Show entity selects. The registry cache can
// briefly come back empty; keep the last name seen so the options stay stable
// (every change of options makes Home Assistant reconnect).
inline std::string stable_title(const std::string &entity) {
  static std::map<std::string,std::string> known;
  if(entity.empty())return "";
  auto title=custom_logic::ha_entity_title_from_registry_cache(entity);
  if(!title.empty()){known[entity]=title;return title;}
  const auto it=known.find(entity);
  return it!=known.end()?it->second:entity;
}
inline std::vector<std::string> page_titles() {
  std::vector<std::string> out;
  for(int p=0;p<stored.count && p<int(MAX_PAGES);++p) {
    const auto &page=stored.pages[p];
    std::string title=page.name;
    if(title.empty())title=stable_title(entry_entity(lead(page)));
    if(title.empty())title="Page "+std::to_string(p+1);
    if(title.size()>60)title.resize(60);
    std::string unique=title;
    for(int n=2;std::find(out.begin(),out.end(),unique)!=out.end();++n)unique=title+" ("+std::to_string(n)+")";
    out.push_back(unique);
  }
  return out;
}
// "Show entity" select options: every configured entity by name, in slot
// order, each paired with its configuration slot. Names are made unique.
inline std::vector<std::pair<std::string,int>> entity_titles() {
  std::vector<std::pair<std::string,int>> out;
  const size_t limit=custom_logic::active_slot_limit();
  for(size_t i=0;i<limit;++i) {
    const auto e=custom_logic::cached_device_entity_id(i);if(e.empty())continue;
    std::string title=stable_title(e);
    if(title.size()>60)title.resize(60);
    std::string unique=title;
    for(int n=2;std::find_if(out.begin(),out.end(),[&](const auto &o){return o.first==unique;})!=out.end();++n)unique=title+" ("+std::to_string(n)+")";
    out.emplace_back(unique,int(i));
  }
  return out;
}
inline int page_of_entry(int entry) {
  if(entry<0)return -1;
  for(int p=0;p<stored.count && p<int(MAX_PAGES);++p)for(auto e:stored.pages[p].entries)if(e==entry)return p;
  return -1;
}
inline void hide() {
  hide_carousel_clock();
  onx_panel_detail::leave_main_page();
  onx_panel_detail::hide();
  showing=false;if(layer)lv_obj_add_flag(layer,LV_OBJ_FLAG_HIDDEN);
  if(suspended_page)restore_underlay();
}
inline void label(lv_obj_t *obj,const std::string &text,int x,int y,int w,const lv_font_t *font) {
  onx_render::style_text_font(obj,font,0);onx_render::label(obj,text.c_str());
  lv_obj_set_pos(obj,x,y);lv_obj_set_width(obj,w);
  if(text.empty())lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(obj,LV_OBJ_FLAG_HIDDEN);
}
inline std::string grid_badge(const std::string &cell, const Item &v, const custom_logic_entity_sub::EntityDetailState *d) {
  if (!d || !d->available || !d->received || cell.empty()) return "";
  using custom_logic_entity_sub::has_prefix;
  if (has_prefix(cell, "light.")) {
    if (!v.on) return "";
    if (onx_panel_detail::light_state) {
      const auto st = onx_panel_detail::light_state(cell);
      if (st.supports_brightness() && st.brightness_pct > 0) return std::to_string(st.brightness_pct) + "%";
    }
    const float b = onx_panel_detail::num(d->brightness);
    if (std::isfinite(b) && b > 0) return std::to_string(int(std::lround(b * 100.0f / 255.0f))) + "%";
    return "";
  }
  if (has_prefix(cell, "fan.")) {
    if (!v.on) return "";
    const float p = onx_panel_detail::num(onx_controls::effective(*d, "percentage"));
    return std::isfinite(p) && p > 0 ? std::to_string(int(std::lround(p))) + "%" : "";
  }
  if (has_prefix(cell, "cover.")) {
    const float p = onx_panel_detail::num(d->position);
    return std::isfinite(p) ? std::to_string(int(std::lround(p))) + "%" : "";
  }
  if (has_prefix(cell, "climate.")) {
    if (!v.on) return "Off";
    const float t = onx_panel_detail::num(onx_controls::effective(*d, "temperature"));
    if (std::isfinite(t)) return onx_detail_view::compact(onx_controls::effective(*d, "temperature")) + "°";
    const float lo = onx_panel_detail::num(d->low), hi = onx_panel_detail::num(d->high);
    if (std::isfinite(lo) && std::isfinite(hi)) return onx_detail_view::compact(d->low) + "–" + onx_detail_view::compact(d->high) + "°";
    return "";
  }
  if (has_prefix(cell, "sensor.") || has_prefix(cell, "number.") || has_prefix(cell, "input_number.")) {
    const float n = onx_panel_detail::num(d->state);
    const std::string out = onx_panel_detail::present(d->state, d->unit);
    if (std::isfinite(n)) return out.size() <= 10 ? out : "";
    if (!has_prefix(cell, "sensor.")) return "";
  }
  // Text states (text sensors, selects, weather, media, timers): the card's
  // reading when it is short enough for the tile.
  for (const char *pre : {"sensor.", "select.", "input_select.", "input_text.", "weather.", "media_player.", "timer.", "vacuum.", "camera.", "lock.", "alarm_control_panel."}) {
    if (!has_prefix(cell, pre)) continue;
    const std::string s = has_prefix(cell, "timer.") && v.context.find(':') != std::string::npos ? v.context : v.value;
    return s != "—" && s != "--" && onx_media::display_text(s).size() <= 14 ? onx_media::display_text(s) : "";
  }
  return "";
}
inline std::vector<onx_panel_detail::Tile> main_tiles(const std::string &entity, const StoredPage *p, Kind kind,
                                                    const std::array<onx_panel_layout::Rect,MAX_CELLS> &areas) {
  std::vector<onx_panel_detail::Tile> tiles;
  const int cells = p ? size_of(*p) : 1;
  tiles.reserve(cells);
  for (int i = 0; i < cells; ++i) {
    const auto cell = p ? entry_entity(p->entries[i]) : entity;
    const auto v = cell.empty() ? Item{} : item(cell, false);
    const auto &r = areas[i];
    onx_panel_detail::Tile t;
    t.entity=cell;t.name=onx_media::display_text(v.name);t.value=v.value;t.context=v.context;
    t.on=v.on;t.ring=v.ring;t.r={r.x,r.y,r.w,r.h};t.compact=kind==Kind::CONTROL_SENSORS && i>0;
    if(kind==Kind::STACK) {
      const auto row=onx_panel_layout::stack_row_of(stack_code(*p),i);
      t.compact=row==onx_panel_layout::Row::COMPACT;t.chip=row==onx_panel_layout::Row::CHIPS;t.slot=true;
    }
    t.key=kind==Kind::ACTION_GRID;
    if ((t.key || t.chip) && !cell.empty()) t.badge = grid_badge(cell, v, custom_logic_entity_sub::entity_detail(cell));
    t.icon_only=p && t.key && (p->artwork[i]&ICON_ONLY);
    t.artwork=p?artwork_id(stored,*p,i):0;
    t.effect=p && t.key ? effect_of(p->entries[i]) : 0;
    tiles.push_back(std::move(t));
  }
  return tiles;
}

inline onx_panel_detail::Tile settings_tile(int width, int height) {
  const auto r = onx_panel_layout::geometry(Kind::SINGLE, width, height)[0];
  onx_panel_detail::Tile t;
  t.entity = onx_panel_detail::SETTINGS;
  t.name = "Settings";
  t.r = {r.x, r.y, r.w, r.h};
  return t;
}

template<typename Bind> void render(const std::string &entity,int width,int height,bool detail,Bind bind) {
  if(!layer)return;
  if (detail) {
    hide_carousel_clock();
    onx_panel_detail::leave_main_page();
  }
  if (detail) suspend_underlay();
  else if (suspended_page) restore_underlay();
  onx_render::style_bg_color(layer,lv_color_hex(onx_display_settings::background()),0);
  if(detail || page_lead!=entity)onx_panel_detail::animated_key.clear();
  const auto *p=detail?nullptr:led_by(entity);
  const auto kind=p?Kind(p->kind):Kind::SINGLE;
  action_grid=kind==Kind::ACTION_GRID;
  page_lead=entity;
  rects=page_geometry(p,width,height,detail);
  count=p?size_of(*p):1;
  for(int i=0;i<count;++i)entities[i]=p?entry_entity(p->entries[i]):entity;
  lv_obj_set_size(layer,width,height);lv_obj_set_pos(layer,0,0);
  // Keep the dock, dashboard and bell above the opaque panel content.
  if(lv_obj_get_index(layer)!=0)lv_obj_move_background(layer);lv_obj_remove_flag(layer,LV_OBJ_FLAG_HIDDEN);showing=true;detail_view=detail;
  if(!onx_panel_detail::bind_icon)onx_panel_detail::bind_icon=[bind](lv_obj_t *icon,const std::string &target){bind(icon,target);};
  // Redesigned full-screen detail screens with direct touch controls.
  if(detail && onx_panel_detail::supports(entity)) {
    for(auto &c:cards){lv_obj_add_flag(c.box,LV_OBJ_FLAG_HIDDEN);c.initialized=false;}
    auto title=custom_logic::ha_entity_title_from_registry_cache(entity);if(title.empty())title=entity;
    const auto view=view_for?view_for(entity,true):onx_detail_view::build(entity,title);
    onx_panel_detail::render(layer,entity,onx_media::display_text(title),view,width,height);
    onx_panel_detail::tick(entity);
    return;
  }
  // Main pages: cards drawn by the shared widget engine (onx_panel_detail.h).
  for(auto &c:cards){lv_obj_add_flag(c.box,LV_OBJ_FLAG_HIDDEN);c.initialized=false;}
  if (!detail) onx_panel_detail::activate_main_page(entity);
  onx_panel_detail::render_main(layer, main_tiles(entity, p, kind, rects), width, height);
  if (!detail) refresh_carousel_clock(width);
}
// The carousel's Settings item, on the same base as the other main pages.
inline void render_settings(int width,int height) {
  if(!layer)return;
  if (suspended_page) restore_underlay();
  onx_render::style_bg_color(layer,lv_color_hex(onx_display_settings::background()),0);
  for(auto &c:cards){lv_obj_add_flag(c.box,LV_OBJ_FLAG_HIDDEN);c.initialized=false;}
  rects=onx_panel_layout::geometry(Kind::SINGLE,width,height,false);
  action_grid=false;
  entities[0]=onx_panel_detail::SETTINGS;count=1;
  lv_obj_set_size(layer,width,height);lv_obj_set_pos(layer,0,0);
  if(lv_obj_get_index(layer)!=0)lv_obj_move_background(layer);
  lv_obj_remove_flag(layer,LV_OBJ_FLAG_HIDDEN);showing=true;detail_view=false;
  onx_panel_detail::activate_main_page(onx_panel_detail::SETTINGS);
  onx_panel_detail::render_main(layer, {settings_tile(width, height)}, width, height);
  refresh_carousel_clock(width);
}
// Pre-warm a neighbor main page while idle (PSRAM grid buffer or hidden RetainedPage).
inline bool prewarm(const std::string &entity, int width, int height) {
  if (!layer || entity.empty() || !onx_panel_detail::bind_icon) return false;
  const auto *p = led_by(entity);
  const auto kind = p ? Kind(p->kind) : Kind::SINGLE;
  const auto areas = page_geometry(p, width, height, false);
  return onx_panel_detail::prewarm_main_page(layer, entity, main_tiles(entity, p, kind, areas), width, height);
}
// The panel's own Assist timers: full screen, above the dock.
// raise: only when the timer screen first appears; raising it on every pass
// fought the notification banner for the top of the layer (flicker).
inline void show_timers(int width,int height,bool raise=true) {
  if(!layer)return;
  hide_carousel_clock();
  onx_panel_detail::leave_main_page();
  suspend_underlay();
  onx_render::style_bg_color(layer,lv_color_hex(onx_display_settings::background()),0);
  for(auto &c:cards){lv_obj_add_flag(c.box,LV_OBJ_FLAG_HIDDEN);c.initialized=false;}
  lv_obj_set_size(layer,width,height);lv_obj_set_pos(layer,0,0);
  if(raise&&lv_obj_get_index(layer)!=int32_t(lv_obj_get_child_count(lv_obj_get_parent(layer)))-1)lv_obj_move_foreground(layer);
  lv_obj_remove_flag(layer,LV_OBJ_FLAG_HIDDEN);showing=true;detail_view=true;
  onx_panel_detail::render_timers(layer,width,height);
}
inline bool tap(int x,int y,bool held=false) {
  if(held && (!showing || detail_view || !action_grid))return false;
  if(showing && detail_view)return onx_panel_detail::active;  // Only its own controls act.
  if(!showing)return false;
  // Settings: the shared press action opens the menu.
  if(entities[0]==onx_panel_detail::SETTINGS)return false;
  for(int i=0;i<count;++i) {
    const auto r=rects[i];if(x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h) {
      const auto &entity=entities[i];
      if(entity.empty())return true;
      if(configured(entity)<0)return true;
      onx_panel_ui::request_render();
      if(action_grid && !held) {
        if(run_grid_action){onx_panel_detail::animate_key(entity);run_grid_action(entity);}
        return true;
      }
      if(open){parent_entity=page_lead;open(entity);}
      return true;
    }
  }
  return action_grid && y>=onx_panel_layout::MAIN_TOP && y<onx_panel_ui::height-52;
}
}
