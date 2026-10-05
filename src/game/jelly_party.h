#pragma once
#include "math/jelly_party.h"
#include "game/roster.h"
#include "game/mmd_audio.h"
#include <future>
#include <fstream>
#include <unordered_set>

namespace poser_jelly {
struct Target {
  void* transform=nullptr;
  void* go=nullptr;
  Vec3 scale{1,1,1};
  Quat rotation{0,0,0,1};
  bool rotationOwned=false;
  double spinAngle=0;
  float randomSign=1;
  bool device=false;
  std::shared_ptr<GripReferences> refs;
};
struct Loaded {std::shared_ptr<const mmd::AudioClip> clip;std::string path,error;};
static std::vector<Target> targets;
static bool TargetsEmpty(){return targets.empty();}
static mmd::AudioPlayer audio;
static std::future<Loaded> loader;
static std::atomic<bool> cancel{false};
static std::atomic<HWND> dialog{nullptr};
static bool loading=false,active=false,configured=false;
static bool startPending=false,stopPending=false,rescanPending=false;
static bool spinWanted=false,spinActive=false;
static float spinSpeed=90.f;
static int spinDirection=0;
static ULONGLONG spinLast=0;
static uint64_t spinSession=0;
static float amplitude=.5f,period=.65f,volume=.7f;
static ULONGLONG started=0,nextScan=0;
static char music[4096]{};
static std::string loadedPath,status=u8"果冻派对未开启";
static size_t scanned=0,failed=0,restored=0;
static void* getScene=nullptr;
static void* rendererClass=nullptr;
static void* getStatic=nullptr;
static void* getBatched=nullptr;
static size_t deviceScanned=0;
static UINT_PTR CALLBACK DialogHook(HWND h,UINT msg,WPARAM,LPARAM) {
  if(msg==WM_INITDIALOG)dialog.store(GetParent(h));
  return 0;
}
static bool ScaleRead(void* transform,Vec3& out) {
  if(!UnityObjAlive(transform)||!g_transform_get_localScale)return false;
  __try {void* boxed=Invoke(g_transform_get_localScale,transform);if(!boxed)return false;
    out=*(Vec3*)((char*)boxed+16);
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z);
  } __except(1) {return false;}
}
static bool ScaleWrite(void* transform,Vec3 value) {
  if(!UnityObjAlive(transform)||!g_transform_set_localScale)return false;
  __try {void* args[]={&value};void* exception=nullptr;
    il2cpp_runtime_invoke(g_transform_set_localScale,transform,args,&exception);return exception==nullptr;}
  __except(1) {return false;}
}
static bool RotationRead(void* transform,Quat& out) {
  if(!UnityObjAlive(transform)||!g_transform_get_localRotation)return false;
  __try {void* boxed=Invoke(g_transform_get_localRotation,transform);if(!boxed)return false;
    out=*(Quat*)((char*)boxed+16);
    return std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.z)&&
           std::isfinite(out.w)&&QuatLen(out)>.5f;
  }__except(1){return false;}
}
static bool RotationWrite(void* transform,Quat value) {
  if(!UnityObjAlive(transform)||!g_transform_set_localRotation)return false;
  __try {void* args[]={&value};void* exception=nullptr;
    il2cpp_runtime_invoke(g_transform_set_localRotation,transform,args,&exception);return exception==nullptr;
  }__except(1){return false;}
}
static bool RestoreRotation(Target& t) {
  if(!t.rotationOwned)return true;
  if(!UnityObjAlive(t.transform)){t.rotationOwned=false;return true;}
  Quat readback;
  if(!RotationWrite(t.transform,t.rotation)||!RotationRead(t.transform,readback))return false;
  auto a=NormQ(t.rotation),b=NormQ(readback);
  if(std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)<.99999f)return false;
  t.rotationOwned=false;return true;
}
static void Restore() {
  restored=0;
  for(auto it=targets.begin();it!=targets.end();) {
    if(!UnityObjAlive(it->transform)){it=targets.erase(it);continue;}
    Vec3 readback;
    if(RestoreRotation(*it)&&ScaleWrite(it->transform,it->scale)&&ScaleRead(it->transform,readback)&&
       std::abs(readback.x-it->scale.x)<.0001f&&std::abs(readback.y-it->scale.y)<.0001f&&
       std::abs(readback.z-it->scale.z)<.0001f) {
      ++restored;it=targets.erase(it);
    }else {++failed;++it;}
  }
  audio.stop();active=false;spinWanted=false;spinActive=false;startPending=false;rescanPending=false;
  Log("[JELLY] stopped, restored %zu visual transforms",restored);
}
static void Configure() {
  if(configured)return;configured=true;
  try {std::ifstream f(MmdConfigDirectory()/L"jelly-party.json");nlohmann::json j;
    if(f){f>>j;auto path=j.value("music",std::string{});strncpy_s(music,path.c_str(),_TRUNCATE);}
  }catch(...) {status=u8"音乐配置读取失败，可重新选择文件";}
}
static void SaveMusic() {
  try {auto dir=MmdConfigDirectory();std::filesystem::create_directories(dir);
    std::ofstream f(dir/L"jelly-party.json",std::ios::binary);
    f<<nlohmann::json({{"music",music}}).dump(2);
  }catch(...) {Log("[JELLY] could not save local music selection");}
}
static void Load(bool browse) {
  if(loading||active)return;
  Configure();const std::string requested=music;HWND owner=g_gameHwnd;
  cancel.store(false);loading=true;status=u8"正在打开音乐…";
  loader=std::async(std::launch::async,[requested,browse,owner]() {
    Loaded result;
    try {
      auto path=std::filesystem::u8path(requested);
      if(browse) {
        wchar_t file[32768]{};OPENFILENAMEW ofn{};ofn.lStructSize=sizeof(ofn);
        ofn.hwndOwner=owner;ofn.lpstrFile=file;ofn.nMaxFile=32768;
        ofn.lpstrFilter=L"Music\0*.wav;*.mp3;*.m4a;*.aac;*.wma;*.flac\0All files\0*.*\0\0";
        ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER|OFN_ENABLEHOOK;
        ofn.lpfnHook=DialogHook;
        if(!GetOpenFileNameW(&ofn)){dialog.store(nullptr);return result;}
        dialog.store(nullptr);path=file;
      }
      result.path=mmd::Utf8(path.wstring());
      if(result.path.empty())throw std::runtime_error("Select a local music file first");
      result.clip=mmd::DecodeAudio(path,cancel);
      if(result.clip->duration()<=0)throw std::runtime_error("Music has no playable samples");
    }catch(const std::exception& e){dialog.store(nullptr);result.error=e.what();}
    return result;
  });
}
static bool HasSelectedAncestor(void* transform,const std::unordered_set<void*>& selected) {
  for(int depth=0;depth<64&&UnityObjAlive(transform)&&g_transform_get_parent;++depth) {
    transform=Invoke(g_transform_get_parent,transform);
    if(selected.count(transform))return true;
  }
  return false;
}
static bool QueryBool(void* method,void* object,bool& value) {
  if(!method||!UnityObjAlive(object))return false;
  void* result=Invoke(method,object);if(!result)return false;
  value=*(bool*)((char*)result+16);return true;
}
// UI animators and world-space UI belong to the interface, never the party.
static bool IsInterface(void* transform) {
  static void* canvas=nullptr;static void* rect=nullptr;
  if(!canvas||!rect){size_t n=0;auto a=il2cpp_domain_get_assemblies(il2cpp_domain_get(),&n);
    canvas=FindClass("UnityEngine","Canvas",a,n);rect=FindClass("UnityEngine","RectTransform",a,n);}
  if(!canvas||!rect||!g_rosterGetComponent)return true;
  for(int depth=0;depth<64&&UnityObjAlive(transform);++depth){
    void* go=Invoke(g_component_get_gameObject,transform);
    if(RosterGetComponentOfClass(go,canvas)||RosterGetComponentOfClass(go,rect))return true;
    transform=Invoke(g_transform_get_parent,transform);
  }
  return UnityObjAlive(transform); // an unbounded hierarchy is not admitted
}
static bool ContainsSelected(void* transform,const std::unordered_set<void*>& selected) {
  std::unordered_set<void*> parent{transform};
  for(void* child:selected)if(child==transform||HasSelectedAncestor(child,parent))return true;
  return false;
}
static void* DeviceVisualRoot(void* transform) {
  void* original=transform;
  // The closest instantiated prefab groups the device's visual pieces, rather
  // than selecting the map or a shared factory parent higher in the hierarchy.
  for(int depth=0;depth<12&&UnityObjAlive(transform);++depth) {
    void* go=Invoke(g_component_get_gameObject,transform);char name[256]{};
    RosterGoName(go,name,sizeof(name));
    if(strstr(name,"(Clone)"))return transform;
    transform=Invoke(g_transform_get_parent,transform);
  }
  return original;
}
static void Scan() {
  ResolveRosterApi();scanned=0;deviceScanned=0;
  if(!getScene) {
    size_t count=0;void* domain=il2cpp_domain_get();
    void** assemblies=il2cpp_domain_get_assemblies(domain,&count);
    if(void* cls=FindClass("UnityEngine","GameObject",assemblies,count)) {
      getScene=FindMethod(cls,"get_scene",0);getStatic=FindMethod(cls,"get_isStatic",0);
    }
    rendererClass=FindClass("UnityEngine","Renderer",assemblies,count);
    if(rendererClass)getBatched=FindMethod(rendererClass,"get_isPartOfStaticBatch",0);
  }
  if(!getScene)return;
  if(!g_rosterFindAll||!g_rosterAnimatorClass||!il2cpp_gchandle_new||!g_transform_get_parent)return;
  void* type=il2cpp_type_get_object(il2cpp_class_get_type(g_rosterAnimatorClass));
  if(!type)return;void* args[]={type};void* arr=Invoke(g_rosterFindAll,nullptr,args);
  if(!arr)return;int n=*(int*)((char*)arr+IL2CPP_ARRAY_LEN);
  if(n<0||n>65536)return;void** items=(void**)((char*)arr+IL2CPP_ARRAY_DATA);
  struct Candidate{void* transform;void* go;bool device=false;};std::vector<Candidate> candidates;
  std::unordered_set<void*> selected;
  for(auto& t:targets)if(UnityObjAlive(t.transform))selected.insert(t.transform);
  for(int i=0;i<n&&selected.size()<1024;++i) {
    void* animator=items[i];if(!UnityObjAlive(animator))continue;
    void* go=Invoke(g_component_get_gameObject,animator);if(RosterGoActive(go)!=1)continue;
    // Unity Scene is a value type containing its integer handle. Prefab assets
    // have handle zero even when their child GameObjects report active.
    void* scene=Invoke(getScene,go);
    if(!scene||*(int*)((char*)scene+16)==0)continue;
    void* transform=SafeGetComponentTransform(animator);
    if(!UnityObjAlive(transform)||IsInterface(transform))continue;
    ++scanned;if(!selected.insert(transform).second)continue;
    candidates.push_back({transform,go});
  }
  if(rendererClass&&getStatic&&getBatched) {
    void* rt=il2cpp_type_get_object(il2cpp_class_get_type(rendererClass));
    if(rt) {
      void* rendererArgs[]={rt};void* renderers=Invoke(g_rosterFindAll,nullptr,rendererArgs);
      int count=renderers?*(int*)((char*)renderers+IL2CPP_ARRAY_LEN):0;
      void** data=renderers?(void**)((char*)renderers+IL2CPP_ARRAY_DATA):nullptr;
      if(count>=0&&count<=65536)for(int i=0;i<count&&selected.size()<1024;++i) {
        void* renderer=data[i];if(!UnityObjAlive(renderer))continue;
        void* go=Invoke(g_component_get_gameObject,renderer);
        if(RosterGoActive(go)!=1)continue;
        bool staticObject=false,batched=false;
        if(!QueryBool(getStatic,go,staticObject)||staticObject||!QueryBool(getBatched,renderer,batched)||batched)continue;
        void* scene=Invoke(getScene,go);if(!scene||*(int*)((char*)scene+16)==0)continue;
        void* transform=SafeGetComponentTransform(renderer);if(!UnityObjAlive(transform))continue;
        if(IsInterface(transform)||selected.count(transform)||HasSelectedAncestor(transform,selected))continue;
        ++deviceScanned;
        transform=DeviceVisualRoot(transform);
        if(!UnityObjAlive(transform)||IsInterface(transform)||HasSelectedAncestor(transform,selected)||ContainsSelected(transform,selected))continue;
        void* rootGo=Invoke(g_component_get_gameObject,transform);
        if(!selected.insert(transform).second)continue;
        candidates.push_back({transform,rootGo,true});
      }
    }
  }
  // If a newly discovered parent owns an existing child, hand the visual
  // scaling lease to the parent after restoring that child's original scale.
  for(auto it=targets.begin();it!=targets.end();) {
    if(IsInterface(it->transform)||HasSelectedAncestor(it->transform,selected)) {
      if(!RestoreRotation(*it)||!ScaleWrite(it->transform,it->scale)){++failed;++it;continue;}
      it=targets.erase(it);
    }else ++it;
  }
  // A child Animator must not multiply an already scaled parent Animator.
  for(auto& c:candidates) {
    if(HasSelectedAncestor(c.transform,selected))continue;
    Vec3 baseline;if(!ScaleRead(c.transform,baseline))continue;
    Target t;t.transform=c.transform;t.go=c.go;t.scale=baseline;t.device=c.device;
    t.refs=std::make_shared<GripReferences>();
    for(void* obj:{c.transform,c.go}) {
      uint32_t handle=il2cpp_gchandle_new(obj,false);
      if(handle)t.refs->handles.push_back(handle);
    }
    if(t.refs->handles.size()==2) {
      if(c.device){char name[256]{};RosterGoName(c.go,name,sizeof(name));Log("[JELLY] device/prop visual target: %s",name);}
      targets.push_back(std::move(t));
    }
  }
  static size_t lastCount=0;
  if(lastCount!=targets.size()){lastCount=targets.size();Log("[JELLY] scanned %zu animators + %zu device renderers; tracking %zu transforms",scanned,deviceScanned,targets.size());}
}
static void Tick() {
  Configure();
  if(loading&&loader.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
    auto result=loader.get();loading=false;
    if(!result.error.empty())status=u8"音乐加载失败："+result.error;
    else if(result.clip){audio.setClip(result.clip);loadedPath=result.path;
      strncpy_s(music,result.path.c_str(),_TRUNCATE);SaveMusic();status=u8"音乐已就绪，点击开始派对";}
    else status=u8"已取消音乐选择";
  }
  if(stopPending){stopPending=false;Restore();status=TargetsEmpty()?u8"已停止，缩放与朝向已恢复":u8"部分模型恢复失败，请再次点击停止复原";}
  if(startPending) {
    startPending=false;if(!audio.clip()){status=u8"请先载入本地音乐";return;}
    const bool keepSpin=spinWanted;
    Restore();
    if(!TargetsEmpty()){status=u8"请先停止并完成模型复原";return;}
    spinWanted=keepSpin;Scan();
    if(TargetsEmpty()){status=u8"未找到活动模型，请进入角色场景后重试";return;}
    started=GetTickCount64();nextScan=started+5000;failed=0;active=true;status=u8"果冻派对进行中";
  }
  const ULONGLONG now=GetTickCount64();
  if(!spinWanted&&spinActive) {
    for(auto& t:targets)if(!RestoreRotation(t)){++failed;Restore();status=u8"朝向恢复失败，请点击停止复原";return;}
    spinActive=false;
  }
  if(spinWanted&&!spinActive) {
    if(TargetsEmpty())Scan();
    if(TargetsEmpty()){spinWanted=false;status=u8"未找到活动模型，请进入场景后重试";return;}
  }
  if(spinWanted&&!spinActive) {
    for(auto& t:targets)t.spinAngle=0;
    ++spinSession;spinActive=true;spinLast=now;nextScan=now+5000;
  }
  if(!active&&!spinActive) {
    if(!TargetsEmpty()){Restore();status=TargetsEmpty()?u8"已恢复朝向与缩放":u8"部分模型恢复失败，请点击停止复原";}
    return;
  }
  if(rescanPending||now>=nextScan){rescanPending=false;Scan();nextScan=now+5000;}
  const double spinDt=double(now-spinLast)*.001;
  spinLast=now;
  auto sample=poser_jelly_math::Sample(double(now-started)*.001,amplitude,period);
  for(auto it=targets.begin();it!=targets.end();) {
    if(!UnityObjAlive(it->transform)||RosterGoActive(it->go)!=1) {
      if(UnityObjAlive(it->transform)&&(!RestoreRotation(*it)||!ScaleWrite(it->transform,it->scale))) {
        ++failed;Restore();status=u8"模型恢复失败，请点击停止复原";return;
      }
      it=targets.erase(it);continue;
    }
    Vec3 value{it->scale.x*sample.x,it->scale.y*sample.y,it->scale.z*sample.z};
    if(active&&!ScaleWrite(it->transform,value)) {
      ++failed;Restore();status=u8"模型缩放失败，请点击停止复原";return;
    }
    const auto seed=uint64_t(reinterpret_cast<uintptr_t>(it->transform));
    if(spinActive) {
      if(!it->rotationOwned) {
        if(!RotationRead(it->transform,it->rotation)){++failed;Restore();status=u8"无法读取模型朝向，请点击停止复原";return;}
        it->rotationOwned=true;it->spinAngle=0;
        it->randomSign=poser_jelly_math::Direction(2,seed^(spinSession*0x9e3779b97f4a7c15ULL));
      }
      const float direction=spinDirection==2?it->randomSign:poser_jelly_math::Direction(spinDirection,seed);
      it->spinAngle=poser_jelly_math::AdvanceSpin(it->spinAngle,spinDt,spinSpeed*direction);
      if(!RotationWrite(it->transform,poser_jelly_math::Spin(it->rotation,it->spinAngle))) {
        ++failed;Restore();status=u8"模型旋转失败，请点击停止复原";return;
      }
    }
    ++it;
  }
  if(!active){status=u8"角色自转中";return;}
  try {mmd::Timeline timeline;timeline.state=mmd::PlayState::Playing;
    timeline.seconds=std::fmod(double(now-started)*.001,audio.clip()->duration());
    audio.sync(timeline,true,true,0,volume);
  }catch(const std::exception& e){Restore();status=u8"音乐播放失败："+std::string(e.what());}
}
static void Shutdown() {
  cancel.store(true);if(HWND h=dialog.load())PostMessageW(h,WM_CLOSE,0,0);
  if(loading){loader.wait();loading=false;}
  Restore();audio.close();
}
static void Draw() {
  if(!ImGui::CollapsingHeader(u8"彩蛋：果冻派对"))return;
  ImGui::TextWrapped(u8"实验性彩蛋，仅供娱乐，可能存在 bug。部分模型可能不跟随或与其他功能冲突；异常时请停止并全部复原，必要时重启游戏。");
  Configure();ImGui::TextWrapped(u8"场上活动模型一起伸缩回弹，包括队友、NPC、敌人，也可能包含道具与特效。停止后恢复原始缩放。");
  ImGui::BeginDisabled(loading||active);
  ImGui::InputText(u8"本地音乐",music,sizeof(music));
  if(ImGui::Button(u8"选择音乐文件"))Load(true);
  ImGui::SameLine();if(ImGui::Button(u8"载入音乐"))Load(false);
  ImGui::EndDisabled();
  ImGui::SliderFloat(u8"果冻幅度",&amplitude,.1f,.6f,"%.2f");
  ImGui::SliderFloat(u8"回弹周期（秒）",&period,.2f,3,"%.2f");
  ImGui::SliderFloat(u8"派对音量",&volume,0,1,"%.2f");
  ImGui::BeginDisabled(loading||active||!audio.clip()||(!TargetsEmpty()&&!spinActive));
  if(ImGui::Button(u8"开始果冻派对"))startPending=true;
  ImGui::EndDisabled();ImGui::SameLine();
  if(ImGui::Button(u8"停止并全部复原"))stopPending=true;
  if(ImGui::Button(spinWanted?u8"停止角色自转":u8"角色自转"))spinWanted=!spinWanted;
  ImGui::Combo(u8"自转方向",&spinDirection,u8"正转\0反转\0随机正反转\0");
  ImGui::SliderFloat(u8"自转速度（度／秒）",&spinSpeed,0,360,"%.0f");
  ImGui::TextWrapped(u8"随机正反转在每个模型开始自转时选定方向，期间保持固定；关闭自转恢复原朝向。自转可单独开启，无需音乐。");
  ImGui::TextWrapped("%s",status.c_str());
  size_t devices=0;for(const auto& t:targets)if(t.device)++devices;
  ImGui::TextDisabled(u8"活动目标：%zu，设备／道具：%zu",targets.size(),devices);
  if(ImGui::Button(u8"重新扫描设备"))rescanPending=true;
  ImGui::TextWrapped(u8"音乐仅从本机播放，不随发布包分发。隐藏面板不会停止派对。");
}
}
