#include <Services/DebugService.h>
#include <imgui.h>
#include <inttypes.h>
#include <PlayerCharacter.h>
#include <Services/PlayerService.h>
#include <Combat/CombatController.h>
#include <Games/ActorExtension.h>
#include <Forms/TESObjectCELL.h>
#include <Combat/CombatGroup.h>
#include <Combat/CombatTargetSelector.h>
#include <Games/Misc/BGSWorldLocation.h>
#include <Combat/CombatInventory.h>
#include <Events/ResEvent.h>

#include <math.h>
#include "ComponentView.cpp"
#include <TimeManager.h>
#include <Messages/RequestOwnershipClaim.h>

namespace
{
struct DetectionState : public NiRefObject
{
    ~DetectionState() override;

    uint32_t level;
    uint8_t unk14;
    uint8_t unk15;
    uint8_t unk16;
    uint8_t pad17;
    float unk18;
    NiPoint3 unk1C;
    float unk28;
    NiPoint3 unk2C;
    float unk38;
    NiPoint3 unk3C;
};
static_assert(sizeof(DetectionState) == 0x48);

BGSEncounterZone* GetLocationEncounterZone(Actor* apActor)
{
    TP_THIS_FUNCTION(TGetLocationEncounterZone, BGSEncounterZone*, Actor);
    POINTER_SKYRIMSE(TGetLocationEncounterZone, getLocationEncounterZone, 20203);
    return TiltedPhoques::ThisCall(getLocationEncounterZone, apActor);
}

bool IsValidTarget(CombatTargetSelector* apThis, Actor* apAttacker, Actor* apTarget, BGSEncounterZone* apEncounterZone)
{
    TP_THIS_FUNCTION(TIsValidTarget, bool, CombatTargetSelector, Actor* apAttacker, Actor* apTarget,
                     BGSEncounterZone* apEncounterZone);
    POINTER_SKYRIMSE(TIsValidTarget, isValidTarget, 47196);
    return TiltedPhoques::ThisCall(isValidTarget, apThis, apAttacker, apTarget, apEncounterZone);
}

DetectionState* GetDetectionState(Actor* apAttacker, Actor* apTarget)
{
    TP_THIS_FUNCTION(TGetDetectionState, DetectionState*, Actor, Actor* apTarget);
    POINTER_SKYRIMSE(TGetDetectionState, getDetectionState, 37757);
    return TiltedPhoques::ThisCall(getDetectionState, apAttacker, apTarget);
}

float GetCombatTargetSelectorDetectionTimeLimit()
{
    POINTER_SKYRIMSE(float, s_value, 382393);
    return *s_value;
}

float GetCombatTargetSelectorRecentLOSTimeLimit()
{
    POINTER_SKYRIMSE(float, s_value, 382400);
    return *s_value;
}

bool ActorHasEquippedRangedWeapon_m(Actor* apActor)
{
    TP_THIS_FUNCTION(TActorHasEquippedRangedWeapon_m, bool, Actor);
    POINTER_SKYRIMSE(TActorHasEquippedRangedWeapon_m, actorHasEquippedRangedWeapon_m, 47303);
    return TiltedPhoques::ThisCall(actorHasEquippedRangedWeapon_m, apActor);
}

BGSWorldLocation* RefrGetWorldLocation(TESObjectREFR* apThis, BGSWorldLocation* apResult)
{
    TP_THIS_FUNCTION(TRefrGetWorldLocation, BGSWorldLocation*, TESObjectREFR, BGSWorldLocation*);
    POINTER_SKYRIMSE(TRefrGetWorldLocation, refrGetWorldLocation, 19784);
    return TiltedPhoques::ThisCall(refrGetWorldLocation, apThis, apResult);
}

float GetModifiedDistance(BGSWorldLocation* apThis, BGSWorldLocation* apWorldLocation)
{
    TP_THIS_FUNCTION(TGetModifiedDistance, float, BGSWorldLocation, BGSWorldLocation*);
    POINTER_SKYRIMSE(TGetModifiedDistance, getModifiedDistance, 18518);
    return TiltedPhoques::ThisCall(getModifiedDistance, apThis, apWorldLocation);
}

float GetDword_142FE5B78()
{
    POINTER_SKYRIMSE(float, s_value, 405282);
    return *s_value;
}

bool CheckMovement(CombatController* apThis, BGSWorldLocation* apWorldLocation)
{
    TP_THIS_FUNCTION(TCheckMovement, bool, CombatController, BGSWorldLocation*);
    POINTER_SKYRIMSE(TCheckMovement, checkMovement, 33261);
    return TiltedPhoques::ThisCall(checkMovement, apThis, apWorldLocation);
}

bool sub_1407E7A40(Actor* apThis, BGSWorldLocation* apWorldLocation)
{
    TP_THIS_FUNCTION(TFunc, bool, Actor, BGSWorldLocation*);
    POINTER_SKYRIMSE(TFunc, func, 47307);
    return TiltedPhoques::ThisCall(func, apThis, apWorldLocation);
}

bool IsFleeing(Actor* apThis)
{
    TP_THIS_FUNCTION(TFunc, bool, Actor);
    POINTER_SKYRIMSE(TFunc, func, 37577);
    return TiltedPhoques::ThisCall(func, apThis);
}

float CalculateTargetScore(CombatTargetSelector* apThis, CombatTarget* apCombatTarget, Actor* apAttacker, Actor* apTarget, BGSEncounterZone* apEncounterZone)
{
    float score = 0.f;

    CombatController* pCombatController = apThis->pCombatController;
    Actor* pCachedAttacker = pCombatController->pCachedAttacker;

    BGSWorldLocation pAttackerWorldLocation{};
    RefrGetWorldLocation(apAttacker, &pAttackerWorldLocation);
    BGSWorldLocation pTargetWorldLocation{};
    RefrGetWorldLocation(apTarget, &pTargetWorldLocation);

    float aiTime = AITimer::GetAITime();

    auto* pDetectionState = GetDetectionState(apAttacker, apTarget);
    if (pDetectionState && (aiTime - pDetectionState->unk38) <= GetCombatTargetSelectorDetectionTimeLimit())
    {
        if (pDetectionState->unk15)
            score = 1000.f;
        else if (pCachedAttacker == apTarget &&
                 (aiTime - pDetectionState->unk18) < GetCombatTargetSelectorRecentLOSTimeLimit())
            score = 1000.f;

        if (pDetectionState->unk14)
            score += 100.f;
    }

    if (pCachedAttacker == apTarget)
        score += 100.f;

    if (ActorHasEquippedRangedWeapon_m(apAttacker) == ActorHasEquippedRangedWeapon_m(apTarget))
        score += 100.f;

    bool isSameWorldLocation = pTargetWorldLocation.pSpace == pAttackerWorldLocation.pSpace;
    float distanceFactor = 0.f;
    if (isSameWorldLocation)
        distanceFactor = GetModifiedDistance(&pAttackerWorldLocation, &pTargetWorldLocation);
    else if (apTarget != pCachedAttacker)
        distanceFactor = 2048.f;

    score += (float)((float)(1.0 - (float)(fminf(distanceFactor, 2048.f) * 0.00048828125f)) * 1000.f);

    if (apCombatTarget->attackerCount > (pCachedAttacker == apTarget))
        score += GetDword_142FE5B78();

    if (pCombatController->pInventory->maximumRange < 1024.f || !isSameWorldLocation)
    {
        if (!CheckMovement(pCombatController, &pTargetWorldLocation))
            score -= 2000.f;
        if (!isSameWorldLocation && !sub_1407E7A40(apAttacker, &pTargetWorldLocation))
            score -= 2500.f;
    }

    if (apTarget->VirtIsDead(false))
        score -= 500.f;

    if (IsFleeing(apTarget))
        score -= 500.f;

    return score;
}
} // namespace

static float progress = 0.0f;
static float elapsedTime = 0.0f;
static float accumulatedTime = 0.0f;
static float accumulatedTime2 = 0.0f;
uint32 cellid = 0;
void DebugService::DrawCombatView()
{

    // Obt�m o tempo delta do frame atual
    float deltaTime = ImGui::GetIO().DeltaTime;

    // Acumula o tempo decorrido
    accumulatedTime += deltaTime;
    accumulatedTime2 += deltaTime;

    auto player = PlayerCharacter::Get();
    if (player)
    {
        //auto pGameTime = TimeData::Get();

        //auto hour = pGameTime->GameHour->f;
        //int hours = static_cast<int>(hour); // Parte inteira (horas)
        //float decimalPart = hour - hours;   // Parte decimal
        //int minutes = std::round(decimalPart * 60);
        const auto view = m_world.view<FormIdComponent>(entt::exclude<ObjectComponent>);
        auto i = 0;
        Vector<entt::entity> entities(view.begin(), view.end());

        if (accumulatedTime2 >= 3)
        {
            for (auto entity : entities)
            {
                auto& formIdComponent = view.get<FormIdComponent>(entity);
                auto* pForm = TESForm::GetById(formIdComponent.Id);
                auto* pActor = Cast<Actor>(pForm);

                if (!pActor)
                {
                    continue;
                }

                if (!pActor->GetExtension()->IsPlayer() and !pActor->IsPlayerSummon() and !pActor->IsTemporary() and !pActor->IsDead())
                {

                    if (pActor->GetExtension()->IsLocal())
                    {
                        if (const auto pLocalComponent = m_world.try_get<LocalComponent>(entity))
                        {
                            auto& action = pLocalComponent->CurrentAction;

                            if (action.EventName == "Unequip" or action.EventName == "combatStanceStop")
                            {
                                pActor->Reset();
                            }
                        }
                    }
                    if (pActor->GetExtension()->IsRemote())
                    {
                        if (auto* pComponent = m_world.try_get<RemoteAnimationComponent>(entity))
                        {
                            if (pComponent->LastRanAction.EventName == "Unequip" or pComponent->LastRanAction.EventName == "combatStanceStop")
                            {
                                pActor->Reset();
                            }
                        }    
                    }
                }

            }
            accumulatedTime2 = 0;
        }

        for (auto entity : entities)
        {
            i++;

            auto& formIdComponent = view.get<FormIdComponent>(entity);
            auto* pForm = TESForm::GetById(formIdComponent.Id);
            auto* pActor = Cast<Actor>(pForm);

            if (!pActor)
            {
                continue;
            }

            if (!pActor->GetExtension()->IsPlayer() and !pActor->IsDead())
            {
                if (player->GetCombatTarget() == pActor or pActor->IsDragon())
                {
                    if (auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(view.get<FormIdComponent>(entity).Id)))
                    {
                        ImVec2 screenPos{};
                        auto playerPosition = player->position;
                        auto actorPosition = pActor->position;
                        float dx = actorPosition.x - playerPosition.x;
                        float dy = actorPosition.y - playerPosition.y;
                        float dz = actorPosition.z - playerPosition.z;
                        auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                        float minDistance = 10.0f;   
                        float maxDistance = 7000.0f; 
                        float maxScale = 1.0f;       
                        float minScale = 0.1f;    
                        float scale = maxScale - (distance - minDistance) / (maxDistance - minDistance) * (maxScale - minScale);

                        scale = std::clamp(scale, minScale, maxScale);
                        ImVec2 elementSize = ImVec2(100.0f * scale, 5.0f * scale);
                        if (DrawInWorldSpaceHeight(pObject, screenPos, elementSize))
                        {
                            std::string windowName = "EnemyHealth" + std::to_string(i);
                            ImGui::SetNextWindowPos(screenPos);
                            ImGui::Begin(windowName.c_str(), nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

                            float currentHealth = pActor->GetActorValue(ActorValueInfo::kHealth);
                            float maxHealth = pActor->GetActorPermanentValue(ActorValueInfo::kHealth);

                            ImGui::SetWindowFontScale(scale);
                            ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                            float healthPercentage = currentHealth / maxHealth;
                            if (!pActor->baseForm)
                            {
                                ImGui::TextColored(color, "No-Name");
                            }
                            else
                            {
                                ImGui::TextColored(color,pActor->baseForm->GetName());
                            }
                            ImVec4 histoColor = ImVec4(0.522f, 0.106f, 0.106f, 1.0f);
                            if (pActor->IsPlayerSummon())
                            {
                                histoColor = ImVec4(0.18, 0.541, 0.357, 1.0f);
                            }
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, histoColor);

                            ImGui::ProgressBar(healthPercentage, elementSize, "");
                            ImGui::PopStyleColor(); 
      
                            ImGui::End();
                        }
                    }
                }
            }

            if (pActor->GetExtension()->IsPlayer() and !pActor->GetExtension()->IsLocalPlayer())
            {
                if (IsFleeing(pActor))
                {
                    spdlog::warn("ala o mano ta correno");
                }
                pActor->SetActorValue(ActorValueInfo::kCombatHealthRegenMult, 0.0);
              
                if (auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(view.get<FormIdComponent>(entity).Id)))
                {
                    ImVec2 screenPos{};
                    auto playerPosition = player->position;
                    auto actorPosition = pActor->position;
                    float dx = actorPosition.x - playerPosition.x;
                    float dy = actorPosition.y - playerPosition.y;
                    float dz = actorPosition.z - playerPosition.z;
                    auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    float minDistance = 10.0f;   
                    float maxDistance = 7000.0f; 
                    float maxScale = 1.0f;      
                    float minScale = 0.1f;      
                    float scale = maxScale - (distance - minDistance) / (maxDistance - minDistance) * (maxScale - minScale);

                    scale = std::clamp(scale, minScale, maxScale);
                    ImVec2 elementSize = ImVec2(100.0f*scale, 5.0f * scale);
                    if (DrawInWorldSpaceHeight(pObject, screenPos,elementSize))
                    {
                        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

                        if (pActor->actorState.IsBleedingOut())
                        {
                            color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                        }
                        ImGui::SetNextWindowPos(screenPos);
                        std::string windowName = "PlayerName" + std::to_string(i);
                        ImGui::Begin(windowName.c_str(), nullptr,
                                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_AlwaysAutoResize);

                        ImGui::SetWindowFontScale(scale);
                        ImGui::SetWindowSize(ImVec2(ImGui::GetWindowWidth(), 100));
                        ImGui::TextColored(color, pActor->baseForm->GetName());
                        ImGui::SameLine(); 
                        int level = pActor->GetLevel();
                        std::string levelText = "Lv. " + std::to_string(level);
                        ImGui::TextColored(color, levelText.c_str());
                        ImGui::SetWindowFontScale(scale);
                        float currentHealth = pActor->GetActorValue(ActorValueInfo::kHealth);
                        float maxHealth = pActor->GetActorPermanentValue(ActorValueInfo::kHealth);
                        float healthPercentage = currentHealth / maxHealth;
                        ImGui::SetWindowFontScale(0.8f);
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.18, 0.541, 0.357, 1.0f));

                        ImGui::ProgressBar(healthPercentage, elementSize, "");
                        ImGui::PopStyleColor();
                        ImGui::End();
                    }
                }

                if (pActor->actorState.IsBleedingOut() and !player->actorState.IsBleedingOut())
                {

                    auto playerPosition = player->position;
                    auto actorPosition = pActor->position;
                    float dx = actorPosition.x - playerPosition.x;
                    float dy = actorPosition.y - playerPosition.y;
                    float dz = actorPosition.z - playerPosition.z;
                    auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (distance < 200)
                    {
                        if (auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(view.get<FormIdComponent>(entity).Id)))
                        {
                            ImVec2 screenPos{};
                            if (DrawInWorldSpace(pObject, screenPos))
                            {
                                if (GetAsyncKeyState(0x45) & 0x8000)
                                {
                                    elapsedTime += ImGui::GetIO().DeltaTime;
                                    progress = elapsedTime / 2.0f;
                                    if (progress >= 1.0f) 
                                    {
                                        progress = 0.0f;
                                        World::Get().GetRunner().Trigger(ResEvent(pActor->formID, player->formID));
                                    }


                                    ImGui::SetNextWindowPos(screenPos);
                                    std::string windowName = "revive" + std::to_string(i);
                                    ImGui::Begin(windowName.c_str(), nullptr,
                                                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                                                     ImGuiWindowFlags_AlwaysAutoResize);
                                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.18, 0.541, 0.357, 1.0f));
                                    ImGui::ProgressBar(progress, ImVec2(200.0f, 20.0f), "Revivendo...");
                                    ImGui::PopStyleColor(); 
                                    ImGui::End();
                                }
                                else
                                {
                                    progress = 0.0f;
                                    elapsedTime = 0.0f;
                                    ImGui::SetNextWindowPos(screenPos);
                                    std::string windowName = "reviveprompt" + std::to_string(i);
                                    ImGui::Begin(windowName.c_str(), nullptr,
                                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
                                    ImGui::Text("Segure E para ressuscitar");
                                    ImGui::End();
                                }

                            }
                        }
                    }
                    else
                    {
                        progress = 0.0f;
                        elapsedTime = 0.0f;
                    }
                }
            }
        }
        
    
    }
}
