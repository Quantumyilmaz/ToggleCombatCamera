#include "Manager.h"

//Manager* manager;


Settings::Settings* settings = nullptr;
RE::PlayerCamera* plyr_c;
float savedZoomOffset = 0.2f;
uint32_t shouldToggle = 0;

// CAM STUFF
bool listen_gradual_zoom = false;
class OnCameraUpdate {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> hook1{REL::RelocationID(49852, 50784)};  // 84AB90, 876700

        auto& trampoline = SKSE::GetTrampoline();
        _Update = trampoline.write_call<5>(hook1.address() + REL::Relocate(0x1A6, 0x1A6),
                                           Update);  // 84AD36, 8768A6
    }

private:
    static void Update(RE::TESCamera* a_this);

    static inline REL::Relocation<decltype(Update)> _Update;
};
void OnCameraUpdate::Update(RE::TESCamera* a_camera) {
    _Update(a_camera);
    if (listen_gradual_zoom) {
        auto* thirdPersonState = static_cast<RE::ThirdPersonState*>(a_camera->currentState.get());
        if (thirdPersonState->currentZoomOffset < -0.19f) {
            listen_gradual_zoom = false;
            RE::PlayerCamera::GetSingleton()->ForceFirstPerson();
        }
    }
}

bool Is3rdP() { 
    if (plyr_c->IsInFirstPerson()) return false;
    auto thirdPersonState =
        static_cast<RE::ThirdPersonState*>(plyr_c->cameraStates[RE::CameraState::kThirdPerson].get());
    if (thirdPersonState->targetZoomOffset != thirdPersonState->currentZoomOffset &&
        thirdPersonState->targetZoomOffset == -0.2f && listen_gradual_zoom)
        return false;
    else return plyr_c->IsInThirdPerson();
}

void ToggleCam(float extra_offset = 0.f) {
    bool is3rdP = Is3rdP();
    listen_gradual_zoom = false;
    auto thirdPersonState =
        static_cast<RE::ThirdPersonState*>(plyr_c->cameraStates[RE::CameraState::kThirdPerson].get());
    if (!is3rdP) {
        plyr_c->ForceThirdPerson();
        thirdPersonState->targetZoomOffset = savedZoomOffset + extra_offset;
    } else if (is3rdP) {
        if (settings->os[0].second) {
            listen_gradual_zoom = true;
            thirdPersonState->targetZoomOffset = -0.2f;
            return;
        }
        plyr_c->ForceFirstPerson();
    }
};

bool PlayerIsInToggledCam() {
    if (settings->main[0].second && !Is3rdP()) return true;
    else if (!settings->main[0].second && Is3rdP())
        return true;
	else return false;
}


// COMBAT TRIGGER STUFF
uint32_t oldstate_c = 0;
uint32_t GetCombatState() { return RE::PlayerCharacter::GetSingleton()->IsInCombat(); }
// weapon draw stuff
uint32_t oldstate_w = 0;
// magic stuff
uint32_t oldstate_m = 0;
bool IsMagicEquipped() {
    auto player_char = RE::PlayerCharacter::GetSingleton();
	auto equipped_obj_L = player_char->GetEquippedObject(true);
    auto equipped_obj_R = player_char->GetEquippedObject(false);
	bool L_is_magic = false; bool R_is_magic = false;
	if (equipped_obj_L) L_is_magic = equipped_obj_L->IsMagicItem();
	if (equipped_obj_R) R_is_magic = equipped_obj_R->IsMagicItem();
	return L_is_magic || R_is_magic;
}

bool IsCasting() {
    if (!IsMagicEquipped()) return false;
    auto player_char = RE::PlayerCharacter::GetSingleton();
    auto equipped_obj_L = player_char->GetEquippedObject(true);
    auto equipped_obj_R = player_char->GetEquippedObject(false);
    RE::MagicItem* equipped_obj_L_MI = nullptr;
    RE::MagicItem* equipped_obj_R_MI = nullptr;
    if (equipped_obj_L) equipped_obj_L_MI = equipped_obj_L->As<RE::MagicItem>();
    if (equipped_obj_R) equipped_obj_R_MI = equipped_obj_R->As<RE::MagicItem>();
    bool is_casting = false;
    if (equipped_obj_L_MI && player_char->IsCasting(equipped_obj_L_MI)) is_casting = true;
    if (equipped_obj_R_MI && player_char->IsCasting(equipped_obj_R_MI)) is_casting = true;
    return is_casting;
}

uint32_t CamSwitchHandling(uint32_t newstate) {
    // Toggle i call lamali miyiz ona bakiyoruz
    if (newstate) {
        if (PlayerIsInToggledCam()) {
            return 0;
        }
    } else {
        if (!PlayerIsInToggledCam()) {
            return 0;
        }
        else if (!settings->os[1].second) {
			return 0;
		}
    } 
    return 1;
}

bool bow_cam_switched = false;
bool magic_switched = false;
bool casting_switched = false;
class OnActorUpdate {
public:
    static void Install() {
        REL::Relocation<std::uintptr_t> hook1{REL::RelocationID(36357, 37348)};  // 84AB90, 876700

        auto& trampoline = SKSE::GetTrampoline();
        _Update = trampoline.write_call<5>(hook1.address() + REL::Relocate(0x6D3, 0x674),
                                           Update);  // 84AD36, 8768A6
        logger::info("Hook installed");
    }

private:
    static void Update(RE::Actor* a_actor, float a_zPos, RE::TESObjectCELL* a_cell);

    static inline REL::Relocation<decltype(Update)> _Update;
};
void OnActorUpdate::Update(RE::Actor* a_actor, float a_zPos, RE::TESObjectCELL* a_cell) {
    if (!a_actor) return _Update(a_actor, a_zPos, a_cell);
    if (RE::PlayerCharacter::GetSingleton()->GetGameStatsData().byCharGenFlag.any(RE::PlayerCharacter::ByCharGenFlag::kHandsBound)) return _Update(a_actor, a_zPos, a_cell);
    if (RE::PlayerCharacter::GetSingleton()->GetFormID()!=a_actor->GetFormID()) return _Update(a_actor, a_zPos, a_cell);
    if (!plyr_c->IsInFirstPerson() && !plyr_c->IsInThirdPerson()) return _Update(a_actor, a_zPos, a_cell);
    shouldToggle = 0;
    auto thirdPersonState =
        static_cast<RE::ThirdPersonState*>(plyr_c->cameraStates[RE::CameraState::kThirdPerson].get());
    if (plyr_c->IsInThirdPerson() && thirdPersonState->currentZoomOffset == thirdPersonState->targetZoomOffset &&
        savedZoomOffset != thirdPersonState->currentZoomOffset) {
		savedZoomOffset = thirdPersonState->currentZoomOffset;
        thirdPersonState->savedZoomOffset = savedZoomOffset;
	}
    

    // killmove handling
    if (a_actor->IsInKillMove()) {
        oldstate_c = 1;
        return _Update(a_actor, a_zPos, a_cell);
    }
    else if (RE::PlayerCamera::GetSingleton()->IsInBleedoutMode()) {
		return _Update(a_actor, a_zPos, a_cell);
    }


    // weapon draw handling
    if (settings->main[2].second && !(settings->main[4].second && IsMagicEquipped())) {
        auto weapon_state = static_cast<uint32_t>(a_actor->AsActorState()->GetWeaponState());
        if ((!weapon_state || weapon_state == 3) && oldstate_w != weapon_state) {
            oldstate_w = weapon_state;
            shouldToggle += CamSwitchHandling(oldstate_w);
        }
    }


    // combat handling
    if (settings->main[1].second && GetCombatState() != oldstate_c && ((!bow_cam_switched && !casting_switched) || settings->main[0].second)) {
        oldstate_c = !oldstate_c;
        shouldToggle += CamSwitchHandling(oldstate_c);
    }


    // bow first person aiming handling
    if (settings->main[3].second) {
        auto attack_state = static_cast<uint32_t>(a_actor->AsActorState()->GetAttackState());
        if (attack_state == 8 && Is3rdP()) {
            ToggleCam();
            bow_cam_switched = true;
            return _Update(a_actor, a_zPos, a_cell); 
        } else if (bow_cam_switched && (!attack_state || attack_state == 13) && !Is3rdP() &&
                   settings->os[1].second) {
            ToggleCam();
            bow_cam_switched = false;
            return _Update(a_actor, a_zPos, a_cell); 
        } 
    }


    // magic draw and casting handling
    if (IsMagicEquipped()) {
        
        // magic draw handling
        auto magic_state = static_cast<uint32_t>(a_actor->AsActorState()->GetWeaponState());
        if (settings->main[4].second && oldstate_m != magic_state) {
            oldstate_m = magic_state;
            if ((!magic_state || magic_state == 5) && !Is3rdP() && magic_switched &&
                settings->os[1].second) {
                ToggleCam();
                magic_switched = false;
                return _Update(a_actor, a_zPos, a_cell); 
            } else if ((magic_state == 2 || magic_state == 3) && Is3rdP()) {
                ToggleCam();
                magic_switched = true;
                return _Update(a_actor, a_zPos, a_cell); 
            }
        }
        // magic casting handling
        if (settings->main[5].second) {
            if (IsCasting() && Is3rdP() && (!casting_switched || !settings->os[1].second)) {
                ToggleCam();
                casting_switched = true;
                return _Update(a_actor, a_zPos, a_cell);
            } else if (!IsCasting() && !Is3rdP() && casting_switched && settings->os[1].second) {
                ToggleCam();
                casting_switched = false;
                magic_switched = false;
                return _Update(a_actor, a_zPos, a_cell);

            }
		}
    }

    if (shouldToggle) ToggleCam();


    return _Update(a_actor, a_zPos, a_cell);
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
        case SKSE::MessagingInterface::kNewGame:
            logger::info("New game started.");
            plyr_c = RE::PlayerCamera::GetSingleton();
            if (!plyr_c) {
                logger::info("Player camera is null!");
                logger::error("Player camera is null!");
            }
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            logger::info("Game loaded.");
            plyr_c = RE::PlayerCamera::GetSingleton();
            if (!plyr_c) {
                logger::info("Player camera is null!");
                logger::error("Player camera is null!");
            }
            break;
    }
};



SKSEPluginLoad(const SKSE::LoadInterface *skse) {

    SetupLog();
    SKSE::Init(skse);
    logger::info("Plugin loaded.");
    SKSE::GetMessagingInterface()->RegisterListener(OnMessage);

    // Settings
    auto loaded = Settings::LoadSettings();
    
    if (loaded) logger::info("Settings loaded.");
    else logger::info("Could not load settings.");
    assert(loaded && "Could not load settings from ini file!");
    
    settings = Settings::Settings::GetSingleton();

    // Hooks
    auto& trampoline = SKSE::GetTrampoline();
    bool hook1 = settings->main[1].second || settings->main[2].second || settings->main[3].second || settings->main[4].second || settings->main[5].second;
    bool hook2 = settings->os[0].second;
    // Bunu hook ekledikce update et
    if (hook1 + hook2)
        trampoline.create(14 * (hook1 + hook2));
    // IsInCombat
    if (hook1) {
		logger::info("Toggle in combat enabled. Hooking...");
		OnActorUpdate::Install();
	}
    // Gradual zoom
    if (hook2) {
        logger::info("Gradual zoom enabled. Hooking...");
        OnCameraUpdate::Install();
    }
    return true;
}