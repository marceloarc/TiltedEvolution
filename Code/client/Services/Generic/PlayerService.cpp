#include <Services/PlayerService.h>

#include <World.h>
#include <imgui.h>
#include <Events/UpdateEvent.h>
#include <Events/ConnectedEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Events/GridCellChangeEvent.h>
#include <Events/CellChangeEvent.h>
#include <Events/PlayerDialogueEvent.h>
#include <Events/PlayerLevelEvent.h>
#include <Events/PartyJoinedEvent.h>
#include <Events/MountEvent.h>
#include <Events/PartyLeftEvent.h>
#include <Events/ResEvent.h>
#include <Events/BeastFormChangeEvent.h>

#include <Messages/PlayerRespawnRequest.h>
#include <Messages/NotifyPlayerRespawn.h>
#include <Messages/ShiftGridCellRequest.h>
#include <Messages/EnterExteriorCellRequest.h>
#include <Messages/EnterInteriorCellRequest.h>
#include <Messages/PlayerDialogueRequest.h>
#include <Messages/ResRequest.h>
#include <Messages/NotifyRes.h>
#include <Messages/PlayerLevelRequest.h>

#include <Services/CharacterService.h>

#include <Structs/ServerSettings.h>

#include <PlayerCharacter.h>
#include <Forms/TESObjectCELL.h>
#include <Forms/TESGlobal.h>
#include <Games/Overrides.h>
#include <Games/References.h>
#include <AI/AIProcess.h>
#include <EquipManager.h>
#include <Forms/TESRace.h>
#include <Messages/RequestActorValueChanges.h>
#include <Messages/MountRequest.h>

PlayerService::PlayerService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_dispatcher(aDispatcher)
    , m_transport(aTransport)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&PlayerService::OnUpdate>(this);
    m_connectedConnection = m_dispatcher.sink<ConnectedEvent>().connect<&PlayerService::OnConnected>(this);
    m_disconnectedConnection = m_dispatcher.sink<DisconnectedEvent>().connect<&PlayerService::OnDisconnected>(this);
    m_settingsConnection = m_dispatcher.sink<ServerSettings>().connect<&PlayerService::OnServerSettingsReceived>(this);
    m_notifyRespawnConnection = m_dispatcher.sink<NotifyPlayerRespawn>().connect<&PlayerService::OnNotifyPlayerRespawn>(this);
    m_gridCellChangeConnection = m_dispatcher.sink<GridCellChangeEvent>().connect<&PlayerService::OnGridCellChangeEvent>(this);
    m_cellChangeConnection = m_dispatcher.sink<CellChangeEvent>().connect<&PlayerService::OnCellChangeEvent>(this);
    m_playerDialogueConnection = m_dispatcher.sink<PlayerDialogueEvent>().connect<&PlayerService::OnPlayerDialogueEvent>(this);
    m_playerLevelConnection = m_dispatcher.sink<PlayerLevelEvent>().connect<&PlayerService::OnPlayerLevelEvent>(this);
    m_partyJoinedConnection = aDispatcher.sink<PartyJoinedEvent>().connect<&PlayerService::OnPartyJoinedEvent>(this);
    m_partyLeftConnection = aDispatcher.sink<PartyLeftEvent>().connect<&PlayerService::OnPartyLeftEvent>(this);
    m_resConnection = m_dispatcher.sink<ResEvent>().connect<&PlayerService::OnResEvent>(this);
    m_notifyResConnection = m_dispatcher.sink<NotifyRes>().connect<&PlayerService::OnNotifyRes>(this);
}

void PlayerService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    RunRespawnUpdates(acEvent.Delta);
    RunPostDeathUpdates(acEvent.Delta);
    RunDifficultyUpdates();
    RunLevelUpdates();
    RunBeastFormDetection();
}

void PlayerService::OnConnected(const ConnectedEvent& acEvent) noexcept
{
    // TODO: SkyrimTogether.esm
    TESGlobal* pKillMove = Cast<TESGlobal>(TESForm::GetById(0x100F19));
    pKillMove->f = 0.f;

    TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
    pWorldEncountersEnabled->f = 0.f;
}

void PlayerService::OnDisconnected(const DisconnectedEvent& acEvent) noexcept
{
    PlayerCharacter::Get()->SetDifficulty(m_previousDifficulty);
    m_serverDifficulty = m_previousDifficulty = 6;

    ToggleDeathSystem(false);

    TESGlobal* pKillMove = Cast<TESGlobal>(TESForm::GetById(0x100F19));
    pKillMove->f = 1.f;

    // Restore to the default value (150 in skyrim, 175 in fallout 4)
    float* greetDistance = Settings::GetGreetDistance();
    *greetDistance = 150.f;

    TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
    pWorldEncountersEnabled->f = 1.f;
}

void PlayerService::OnServerSettingsReceived(const ServerSettings& acSettings) noexcept
{
    m_previousDifficulty = *Settings::GetDifficulty();
    PlayerCharacter::Get()->SetDifficulty(acSettings.Difficulty);
    m_serverDifficulty = acSettings.Difficulty;

    if (!acSettings.GreetingsEnabled)
    {
        float* greetDistance = Settings::GetGreetDistance();
        *greetDistance = 0.f;
    }

    ToggleDeathSystem(acSettings.DeathSystemEnabled);
}

void PlayerService::OnResEvent(const ResEvent& acEvent) const noexcept
{
    auto view = m_world.view<FormIdComponent>();

    const auto PlayerIt = std::find_if(std::begin(view), std::end(view), [id = acEvent.PlayerId, view](auto entity) {
        return view.get<FormIdComponent>(entity).Id == id;
    });

    if (PlayerIt == std::end(view))
    {
        spdlog::warn("Player not found, form id: {:X}", acEvent.PlayerId);
        return;
    }

   const entt::entity cPlayerEntity = *PlayerIt;

    std::optional<uint32_t> playerServerIdRes = Utils::GetServerId(cPlayerEntity);
    if (!playerServerIdRes.has_value())
    {
        spdlog::error("{}: failed to find server id", __FUNCTION__);
        return;
    }

    const auto TargetIt = std::find_if(std::begin(view), std::end(view), [id = acEvent.TargetId, view](auto entity) {
        return view.get<FormIdComponent>(entity).Id == id;
    });

    if (TargetIt == std::end(view))
    {
        spdlog::warn("target not found, form id: {:X}", acEvent.TargetId);
        return;
    }

    const entt::entity cTargetEntity = *TargetIt;

    std::optional<uint32_t> targetServerIdRes = Utils::GetServerId(cTargetEntity);
    if (!targetServerIdRes.has_value())
    {
        spdlog::error("{}: failed to find server id", __FUNCTION__);
        return;
    }

    ResRequest request;
    request.TargetId = targetServerIdRes.value();
    request.PlayerId = playerServerIdRes.value();
    m_transport.Send(request);
}

void PlayerService::OnNotifyRes(const NotifyRes& acMessage) const noexcept
{
    Actor* pActor = Utils::GetByServerId<Actor>(acMessage.TargetId);

    if (!pActor)
    {
        return;
    }

    auto pPlayer = PlayerCharacter::Get();

    if (pPlayer)
    {
        if (pActor == pPlayer)
        {
            FadeOutGame(true, true, 3.0f, true,0.1f);
            pPlayer->RespawnPlayer();

            m_knockdownStart = true;

            // m_transport.Send(PlayerRespawnRequest());

            auto* pEquipManager = EquipManager::Get();
            TESForm* pSpell = TESForm::GetById(m_cachedMainSpellId);
            if (pSpell)
                pEquipManager->EquipSpell(pPlayer, pSpell, 0);
            pSpell = TESForm::GetById(m_cachedSecondarySpellId);
            if (pSpell)
                pEquipManager->EquipSpell(pPlayer, pSpell, 1);
            pSpell = TESForm::GetById(m_cachedPowerId);
            if (pSpell)
                pEquipManager->EquipShout(pPlayer, pSpell);

        }
    }
}

void PlayerService::OnNotifyPlayerRespawn(const NotifyPlayerRespawn& acMessage) const noexcept
{
    PlayerCharacter::Get()->PayGold(acMessage.GoldLost);

    std::string message = fmt::format("You died and lost {} gold.", acMessage.GoldLost);
    Utils::ShowHudMessage(String(message));
}

void PlayerService::OnGridCellChangeEvent(const GridCellChangeEvent& acEvent) const noexcept
{
    uint32_t baseId = 0;
    uint32_t modId = 0;

    if (m_world.GetModSystem().GetServerModId(acEvent.WorldSpaceId, modId, baseId))
    {
        ShiftGridCellRequest request;
        request.WorldSpaceId = GameId(modId, baseId);
        request.PlayerCell = acEvent.PlayerCell;
        request.CenterCoords = acEvent.CenterCoords;
        request.Cells = acEvent.Cells;

        m_transport.Send(request);
    }
}

void PlayerService::OnCellChangeEvent(const CellChangeEvent& acEvent) const noexcept
{
    if (acEvent.WorldSpaceId)
    {
        EnterExteriorCellRequest message;
        message.CellId = acEvent.CellId;
        message.WorldSpaceId = acEvent.WorldSpaceId;
        message.CurrentCoords = acEvent.CurrentCoords;

        m_transport.Send(message);
    }
    else
    {
        EnterInteriorCellRequest message;
        message.CellId = acEvent.CellId;

        m_transport.Send(message);
    }
}

void PlayerService::OnPlayerDialogueEvent(const PlayerDialogueEvent& acEvent) const noexcept
{
    if (!m_transport.IsConnected())
        return;

    const auto& partyService = m_world.GetPartyService();
    if (!partyService.IsInParty() || !partyService.IsLeader())
        return;

    PlayerDialogueRequest request{};
    request.Text = acEvent.Text;

    m_transport.Send(request);
}

void PlayerService::OnPlayerLevelEvent(const PlayerLevelEvent& acEvent) const noexcept
{
    if (!m_transport.IsConnected())
        return;

    PlayerLevelRequest request{};
    request.NewLevel = PlayerCharacter::Get()->GetLevel();

    m_transport.Send(request);
}

void PlayerService::OnPartyJoinedEvent(const PartyJoinedEvent& acEvent) noexcept
{
    // TODO: this can be done a bit prettier
    if (acEvent.IsLeader)
    {
        TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
        pWorldEncountersEnabled->f = 1.f;
    }
}

void PlayerService::OnPartyLeftEvent(const PartyLeftEvent& acEvent) noexcept
{
    // TODO: this can be done a bit prettier
    if (World::Get().GetTransport().IsConnected())
    {
        TESGlobal* pWorldEncountersEnabled = Cast<TESGlobal>(TESForm::GetById(0xB8EC1));
        pWorldEncountersEnabled->f = 0.f;
    }
}

void PlayerService::RunRespawnUpdates(const double acDeltaTime) noexcept
{
    if (!m_isDeathSystemEnabled)
        return;

    static bool s_startTimer = false;
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer->actorState.IsBleedingOut())
    {
        m_cachedMainSpellId = pPlayer->magicItems[0] ? pPlayer->magicItems[0]->formID : 0;
        m_cachedSecondarySpellId = pPlayer->magicItems[1] ? pPlayer->magicItems[1]->formID : 0;
        m_cachedPowerId = pPlayer->equippedShout ? pPlayer->equippedShout->formID : 0;

        s_startTimer = false;
        return;
    }

    if (!s_startTimer)
    {
        s_startTimer = true;
        m_respawnTimer = 5.0;
        // If a player dies not by its health reaching 0, getting it up from its bleedout state isn't possible
        // just by setting its health back to max. Therefore, put it to 0.
        if (pPlayer->GetActorValue(ActorValueInfo::kHealth) > 0.f)
            pPlayer->ForceActorValue(ActorValueOwner::ForceMode::DAMAGE, ActorValueInfo::kHealth, 0);


        pPlayer->PayCrimeGoldToAllFactions();
    }

    m_respawnTimer -= acDeltaTime;

    const auto view = m_world.view<FormIdComponent>(entt::exclude<ObjectComponent>);
    auto i = 0;
    auto j = 0;

    Vector<entt::entity> entities(view.begin(), view.end());

    for (auto entity : entities)
    {
        auto& formComponent = view.get<FormIdComponent>(entity);
        const auto pActor = Cast<Actor>(TESForm::GetById(formComponent.Id));

        if (pActor->GetExtension()->IsRemotePlayer())
        {
            i++;

            if (pActor->actorState.IsBleedingOut())
            {
                j++;
            }     
        }
    }

    if (i == j)
    {
        FadeOutGame(true, true, 3.0f, true, 2.0f);
        pPlayer->RespawnPlayerPos();
        m_knockdownStart = true;
       // m_transport.Send(PlayerRespawnRequest());

        s_startTimer = false;
        auto* pEquipManager = EquipManager::Get();
        TESForm* pSpell = TESForm::GetById(m_cachedMainSpellId);
        if (pSpell)
            pEquipManager->EquipSpell(pPlayer, pSpell, 0);
        pSpell = TESForm::GetById(m_cachedSecondarySpellId);
        if (pSpell)
            pEquipManager->EquipSpell(pPlayer, pSpell, 1);
        pSpell = TESForm::GetById(m_cachedPowerId);
        if (pSpell)
            pEquipManager->EquipShout(pPlayer, pSpell);

    }

}

// Doesn't seem to respawn quite yet
void PlayerService::RunPostDeathUpdates(const double acDeltaTime) noexcept
{
    if (!m_isDeathSystemEnabled)
        return;

    // If a player dies in ragdoll, it gets stuck.
    // This code ragdolls the player again upon respawning.
    // It also makes the player invincible for 5 seconds.
    if (m_knockdownStart)
    {
        m_knockdownTimer -= acDeltaTime;
        if (m_knockdownTimer <= 0.0)
        {
            PlayerCharacter::Get()->SetAlpha(0.4);
            PlayerCharacter::SetGodMode(true);
            m_godmodeStart = true;
            m_godmodeTimer = 10.0;
            PlayerCharacter* pPlayer = PlayerCharacter::Get();
            pPlayer->currentProcess->KnockExplosion(pPlayer, &pPlayer->position, 0.f);
            FadeOutGame(false, true, 0.5f, true, 2.f);
         
            m_knockdownTimer = 1.0;
            m_knockdownStart = false;
        }
    }
    if (m_godmodeStart)
    {
        m_godmodeTimer -= acDeltaTime;
        if (m_godmodeTimer <= 0.0)
        {
            PlayerCharacter::Get()->SetAlpha(1);
            PlayerCharacter::SetGodMode(false); 
            m_godmodeStart = false;
        }
    }
}

void PlayerService::RunDifficultyUpdates() const noexcept
{
    if (!m_transport.IsConnected())
        return;

    PlayerCharacter::Get()->SetDifficulty(m_serverDifficulty);
}

void PlayerService::RunLevelUpdates() const noexcept
{
    // The LevelUp hook is kinda weird, so ehh, just check periodically, doesn't really cost anything.

    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 1000ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    static uint16_t oldLevel = PlayerCharacter::Get()->GetLevel();

    uint16_t newLevel = PlayerCharacter::Get()->GetLevel();
    if (newLevel != oldLevel)
    {
        PlayerLevelRequest request{};
        request.NewLevel = newLevel;

        m_transport.Send(request);

        oldLevel = newLevel;
    }
}

void PlayerService::RunBeastFormDetection() const noexcept
{
    static uint32_t lastRaceFormID = 0;
    static std::chrono::steady_clock::time_point lastSendTimePoint;
    constexpr auto cDelayBetweenUpdates = 250ms;

    const auto now = std::chrono::steady_clock::now();
    if (now - lastSendTimePoint < cDelayBetweenUpdates)
        return;

    lastSendTimePoint = now;

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer->race)
        return;
    
    if (pPlayer->race->formID == lastRaceFormID)
        return;

    if (pPlayer->race->formID == 0x200283A || pPlayer->race->formID == 0xCDD84)
        m_world.GetDispatcher().trigger(BeastFormChangeEvent());

    lastRaceFormID = pPlayer->race->formID;
}

void PlayerService::ToggleDeathSystem(bool aSet) noexcept
{
    m_isDeathSystemEnabled = aSet;

    PlayerCharacter::Get()->SetPlayerRespawnMode(aSet);
}
