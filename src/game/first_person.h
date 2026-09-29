#pragma once
#include "core/game_hooks.h"
#include "math/quat_math.h"

#include <atomic>
#include <cmath>
#include <memory>

// 第一人称视角（借用游戏相机，插件不碰输入）
// ---------------------------------------------------------------
// 只做一件事：把游戏主相机搬到"当前角色头骨"的位置。
//   - 鼠标环视、WASD 移动继续由游戏自己处理，插件不读也不发任何输入；
//   - 朝向保持游戏相机的原样，只叠加一个可选的俯仰修正；
//   - 和 MMD 镜头共用同一套接管/还原，MMD 播放时让位。
// UI 线程在 g_poseMutex 下改 settings 并 Publish() 出不可变快照；相机回调
// 不持锁，只读快照和 desired（对齐 mmd_camera 的 published 机制）。
namespace first_person {
constexpr float kPi = 3.14159265358979f;

// 眼睛偏移默认值（右/上/前，米）：实机调出来的对齐值。
static const Vec3 kDefaultOffset{0.f, 0.07f, -0.045f};

struct Settings {
  bool enabled = false;
  // 相对视线的偏移（右/上/前，米）：把相机从"头骨原点"挪到眼睛。
  Vec3 offset = kDefaultOffset;
  // 俯仰修正（度）。第三人称相机通常略微俯视角色，按需往上抬一点。
  float pitch = 0.f;
  // 把头部骨骼缩到 0，避免镜头切在头脸网格里。
  bool hideHead = true;
};

static Settings settings; // 只在持有 g_poseMutex 时改
static std::shared_ptr<const Settings> published = std::make_shared<const Settings>();
static std::atomic<bool> desired{false};

// 在持有 g_poseMutex 时调用（面板勾选 / 滑条 / 复位）。
static void Publish() {
  desired.store(settings.enabled);
  std::atomic_store(&published, std::make_shared<const Settings>(settings));
}
static Settings Snapshot() {
  auto p = std::atomic_load(&published);
  return p ? *p : Settings{};
}

static bool Finite(Vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// 当前角色的头骨变换。切人中 / 没头骨都算失败。
static void *HeadTransform() {
  if (CharacterSwitchInProgress())
    return nullptr;
  void *head = GetHumanoidBone(Head);
  return (head && UnityObjAlive(head)) ? head : nullptr;
}

static bool HeadWorldPosition(Vec3 &out) {
  void *head = HeadTransform();
  if (!head)
    return false;
  Vec3 p = GetBoneWorldPos(head);
  if (!Finite(p) || (std::fabs(p.x) < 1e-4f && std::fabs(p.y) < 1e-4f &&
                     std::fabs(p.z) < 1e-4f))
    return false;
  out = p;
  return true;
}

// 解出本帧相机应当写入的世界位姿。朝向读的是游戏这一帧已经写好的相机朝向，
// 所以鼠标环视仍然是游戏在做，插件只是把相机挪到头骨。
static bool Solve(void *cameraTransform, const Settings &s, Vec3 &position,
                  Quat &rotation) {
  Vec3 head{};
  if (!HeadWorldPosition(head))
    return false;
  Quat q = GetBoneWorldRot(cameraTransform);
  if (!std::isfinite(q.x) || !std::isfinite(q.y) || !std::isfinite(q.z) ||
      !std::isfinite(q.w) || QuatLen(q) < .5f)
    return false;
  if (s.pitch != 0.f) {
    Vec3 right = q * Vec3{1.f, 0.f, 0.f};
    q = NormQ(Quat::AxisAngle(right, s.pitch * kPi / 180.f) * q);
  }
  rotation = q;
  position = head + q * s.offset;
  return Finite(position);
}
} // namespace first_person
