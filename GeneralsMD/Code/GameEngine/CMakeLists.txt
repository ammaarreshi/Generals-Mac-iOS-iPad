set(GAMEENGINE_SRC
#    Include/Common/AcademyStats.h
    Include/Common/ActionManager.h
#    Include/Common/ArchiveFile.h
#    Include/Common/ArchiveFileSystem.h
#    Include/Common/AsciiString.h
#    Include/Common/AudioAffect.h
#    Include/Common/AudioEventInfo.h
#    Include/Common/AudioEventRTS.h
#    Include/Common/AudioHandleSpecialValues.h
#    Include/Common/AudioRandomValue.h
#    Include/Common/AudioRequest.h
#    Include/Common/AudioSettings.h
    Include/Common/BattleHonors.h
    Include/Common/BezFwdIterator.h
    Include/Common/BezierSegment.h
    Include/Common/BitFlags.h
    Include/Common/BitFlagsIO.h
    Include/Common/BorderColors.h
    Include/Common/BuildAssistant.h
    Include/Common/ClientUpdateModule.h
    Include/Common/CommandLine.h
#    Include/Common/crc.h
#    Include/Common/CRCDebug.h
    Include/Common/CriticalSection.h
    Include/Common/CustomMatchPreferences.h
    Include/Common/DamageFX.h
    Include/Common/DataChunk.h
#    Include/Common/Debug.h
    Include/Common/Dict.h
    Include/Common/Directory.h
    Include/Common/DisabledTypes.h
    Include/Common/DiscreteCircle.h
    Include/Common/DrawModule.h
#    Include/Common/DynamicAudioEventInfo.h
    Include/Common/encrypt.h
    Include/Common/Energy.h
#    Include/Common/Errors.h
#    Include/Common/file.h
#    Include/Common/FileSystem.h
    Include/Common/FunctionLexicon.h
#    Include/Common/GameAudio.h
#    Include/Common/GameCommon.h
#    Include/Common/GameDefines.h
    Include/Common/GameEngine.h
    Include/Common/GameLOD.h
#    Include/Common/GameMemory.h
#    Include/Common/GameMusic.h
#    Include/Common/GameSounds.h
    Include/Common/GameSpyMiscPreferences.h
    Include/Common/GameState.h
    Include/Common/GameStateMap.h
#    Include/Common/GameType.h
    Include/Common/Geometry.h
    Include/Common/GlobalData.h
    Include/Common/Handicap.h
    Include/Common/IgnorePreferences.h
#    Include/Common/INI.h
    Include/Common/INIException.h
    Include/Common/KindOf.h
    Include/Common/LadderPreferences.h
    Include/Common/Language.h
    Include/Common/LatchRestore.h
    Include/Common/List.h
#    Include/Common/LocalFile.h
#    Include/Common/LocalFileSystem.h
#    Include/Common/MapObject.h
    Include/Common/MapReaderWriterInfo.h
    Include/Common/MessageStream.h
    Include/Common/MiniLog.h
#    Include/Common/MiscAudio.h
    Include/Common/MissionStats.h
    Include/Common/ModelState.h
    Include/Common/Module.h
    Include/Common/ModuleFactory.h
    Include/Common/Money.h
    Include/Common/MultiplayerSettings.h
    Include/Common/NameKeyGenerator.h
#    Include/Common/ObjectStatusTypes.h
    Include/Common/OSDisplay.h
    Include/Common/Overridable.h
    Include/Common/Override.h
    Include/Common/PartitionSolver.h
    Include/Common/PerfMetrics.h
    Include/Common/PerfTimer.h
    Include/Common/Player.h
    Include/Common/PlayerList.h
    Include/Common/PlayerTemplate.h
    Include/Common/ProductionPrerequisite.h
    Include/Common/QuickmatchPreferences.h
    Include/Common/QuotedPrintable.h
#    Include/Common/Radar.h
#    Include/Common/RAMFile.h
#    Include/Common/RandomValue.h
    Include/Common/Recorder.h
#    Include/Common/ReplaySimulation.h
    Include/Common/Registry.h
    Include/Common/ResourceGatheringManager.h
    Include/Common/Science.h
    Include/Common/ScopedMutex.h
    Include/Common/ScoreKeeper.h
#    Include/Common/simpleplayer.h
    Include/Common/SkirmishBattleHonors.h
    Include/Common/SkirmishPreferences.h
#    Include/Common/Snapshot.h
    Include/Common/SparseMatchFinder.h
    Include/Common/SpecialPower.h
    Include/Common/SpecialPowerMaskType.h
    Include/Common/SpecialPowerType.h
    Include/Common/StackDump.h
    Include/Common/StateMachine.h
    Include/Common/StatsCollector.h
#    Include/Common/STLTypedefs.h
#    Include/Common/StreamingArchiveFile.h
#    Include/Common/SubsystemInterface.h
    Include/Common/Team.h
    Include/Common/Terrain.h
    Include/Common/TerrainTypes.h
    Include/Common/Thing.h
    Include/Common/ThingFactory.h
    Include/Common/ThingSort.h
    Include/Common/ThingTemplate.h
    Include/Common/TunnelTracker.h
#    Include/Common/UnicodeString.h
    Include/Common/UnitTimings.h
    Include/Common/Upgrade.h
#    Include/Common/urllaunch.h
#    Include/Common/UserPreferences.h
    Include/Common/version.h
    Include/Common/WellKnownKeys.h
#    Include/Common/WorkerProcess.h
#    Include/Common/Xfer.h
#    Include/Common/XferCRC.h
#    Include/Common/XferDeepCRC.h
#    Include/Common/XferLoad.h
#    Include/Common/XferSave.h
#    Include/GameClient/Anim2D.h
    Include/GameClient/AnimateWindowManager.h
    Include/GameClient/CampaignManager.h
#    Include/GameClient/ChallengeGenerals.h
#    Include/GameClient/ClientInstance.h
#    Include/GameClient/ClientRandomValue.h
#    Include/GameClient/Color.h
#    Include/GameClient/CommandXlat.h
#    Include/GameClient/ControlBar.h
#    Include/GameClient/ControlBarResizer.h
#    Include/GameClient/ControlBarScheme.h
#    Include/GameClient/Credits.h
#    Include/GameClient/DebugDisplay.h
    Include/GameClient/Diplomacy.h
    Include/GameClient/DisconnectMenu.h
#    Include/GameClient/Display.h
#    Include/GameClient/DisplayString.h
#    Include/GameClient/DisplayStringManager.h
    Include/GameClient/Drawable.h
    Include/GameClient/DrawableInfo.h
#    Include/GameClient/DrawGroupInfo.h
    Include/GameClient/EstablishConnectionsMenu.h
    Include/GameClient/Eva.h
    Include/GameClient/ExtendedMessageBox.h
    Include/GameClient/FontDesc.h
#    Include/GameClient/FXList.h
#    Include/GameClient/Gadget.h
#    Include/GameClient/GadgetCheckBox.h
#    Include/GameClient/GadgetComboBox.h
#    Include/GameClient/GadgetListBox.h
#    Include/GameClient/GadgetProgressBar.h
#    Include/GameClient/GadgetPushButton.h
#    Include/GameClient/GadgetRadioButton.h
#    Include/GameClient/GadgetSlider.h
#    Include/GameClient/GadgetStaticText.h
#    Include/GameClient/GadgetTabControl.h
#    Include/GameClient/GadgetTextEntry.h
    Include/GameClient/GameClient.h
#    Include/GameClient/GameFont.h
    Include/GameClient/GameInfoWindow.h
#    Include/GameClient/GameText.h
#    Include/GameClient/GameWindow.h
#    Include/GameClient/GameWindowGlobal.h
    Include/GameClient/GameWindowID.h
    Include/GameClient/GameWindowManager.h
#    Include/GameClient/GameWindowTransitions.h
#    Include/GameClient/GlobalLanguage.h
#    Include/GameClient/GraphDraw.h
    Include/GameClient/GUICallbacks.h
#    Include/GameClient/GUICommandTranslator.h
#    Include/GameClient/HeaderTemplate.h
#    Include/GameClient/HintSpy.h
#    Include/GameClient/HotKey.h
#    Include/GameClient/Image.h
#    Include/GameClient/IMEManager.h
    Include/GameClient/InGameUI.h
#    Include/GameClient/Keyboard.h
    Include/GameClient/KeyDefs.h
#    Include/GameClient/LanguageFilter.h
#    Include/GameClient/Line2D.h
#    Include/GameClient/LoadScreen.h
#    Include/GameClient/LookAtXlat.h
#    Include/GameClient/MapUtil.h
    Include/GameClient/MessageBox.h
#    Include/GameClient/MetaEvent.h
#    Include/GameClient/Module/AnimatedParticleSysBoneClientUpdate.h
#    Include/GameClient/Module/BeaconClientUpdate.h
#    Include/GameClient/Module/SwayClientUpdate.h
#    Include/GameClient/Mouse.h
#    Include/GameClient/ParabolicEase.h
#    Include/GameClient/ParticleSys.h
#    Include/GameClient/PlaceEventTranslator.h
#    Include/GameClient/ProcessAnimateWindow.h
#    Include/GameClient/RadiusDecal.h
#    Include/GameClient/RayEffect.h
#    Include/GameClient/SelectionInfo.h
#    Include/GameClient/SelectionXlat.h
    Include/GameClient/Shadow.h
    Include/GameClient/Shell.h
    Include/GameClient/ShellHooks.h
    Include/GameClient/ShellMenuScheme.h
#    Include/GameClient/Smudge.h
#    Include/GameClient/Snow.h
#    Include/GameClient/Statistics.h
#    Include/GameClient/TerrainRoads.h
#    Include/GameClient/TerrainVisual.h
#    Include/GameClient/VideoPlayer.h
#    Include/GameClient/View.h
#    Include/GameClient/Water.h
#    Include/GameClient/WindowLayout.h
#    Include/GameClient/WindowVideoManager.h
#    Include/GameClient/WindowXlat.h
#    Include/GameClient/WinInstanceData.h
    Include/GameLogic/AI.h
    Include/GameLogic/AIDock.h
    Include/GameLogic/AIGuard.h
    Include/GameLogic/AIGuardRetaliate.h
#    Include/GameLogic/AIPathfind.h
    Include/GameLogic/AIPlayer.h
    Include/GameLogic/AISkirmishPlayer.h
    Include/GameLogic/AIStateMachine.h
    Include/GameLogic/AITNGuard.h
    Include/GameLogic/Armor.h
    Include/GameLogic/ArmorSet.h
#    Include/GameLogic/CaveSystem.h
#    Include/GameLogic/CrateSystem.h
#    Include/GameLogic/Damage.h
    Include/GameLogic/ExperienceTracker.h
    Include/GameLogic/FiringTracker.h
    Include/GameLogic/FPUControl.h
    Include/GameLogic/GameLogic.h
    Include/GameLogic/GhostObject.h
    Include/GameLogic/Locomotor.h
    Include/GameLogic/LocomotorSet.h
#    Include/GameLogic/LogicRandomValue.h
    Include/GameLogic/Module/ActiveBody.h
    Include/GameLogic/Module/ActiveShroudUpgrade.h
    Include/GameLogic/Module/AIUpdate.h
    Include/GameLogic/Module/AnimationSteeringUpdate.h
    Include/GameLogic/Module/ArmorUpgrade.h
    Include/GameLogic/Module/AssaultTransportAIUpdate.h
    Include/GameLogic/Module/AssistedTargetingUpdate.h
    Include/GameLogic/Module/AutoDepositUpdate.h
    Include/GameLogic/Module/AutoFindHealingUpdate.h
    Include/GameLogic/Module/AutoHealBehavior.h
    Include/GameLogic/Module/BaikonurLaunchPower.h
    Include/GameLogic/Module/BaseRegenerateUpdate.h
    Include/GameLogic/Module/BattleBusSlowDeathBehavior.h
    Include/GameLogic/Module/BattlePlanUpdate.h
    Include/GameLogic/Module/BehaviorModule.h
    Include/GameLogic/Module/BodyModule.h
    Include/GameLogic/Module/BoneFXDamage.h
    Include/GameLogic/Module/BoneFXUpdate.h
    Include/GameLogic/Module/BridgeBehavior.h
    Include/GameLogic/Module/BridgeScaffoldBehavior.h
    Include/GameLogic/Module/BridgeTowerBehavior.h
    Include/GameLogic/Module/BunkerBusterBehavior.h
    Include/GameLogic/Module/CashBountyPower.h
    Include/GameLogic/Module/CashHackSpecialPower.h
    Include/GameLogic/Module/CaveContain.h
    Include/GameLogic/Module/CheckpointUpdate.h
    Include/GameLogic/Module/ChinookAIUpdate.h
    Include/GameLogic/Module/CleanupAreaPower.h
    Include/GameLogic/Module/CleanupHazardUpdate.h
    Include/GameLogic/Module/CollideModule.h
    Include/GameLogic/Module/CommandButtonHuntUpdate.h
    Include/GameLogic/Module/CommandSetUpgrade.h
    Include/GameLogic/Module/ContainModule.h
    Include/GameLogic/Module/ConvertToCarBombCrateCollide.h
    Include/GameLogic/Module/ConvertToHijackedVehicleCrateCollide.h
    Include/GameLogic/Module/CostModifierUpgrade.h
    Include/GameLogic/Module/CountermeasuresBehavior.h
    Include/GameLogic/Module/CrateCollide.h
    Include/GameLogic/Module/CreateCrateDie.h
    Include/GameLogic/Module/CreateModule.h
    Include/GameLogic/Module/CreateObjectDie.h
    Include/GameLogic/Module/CrushDie.h
    Include/GameLogic/Module/DamageModule.h
    Include/GameLogic/Module/DamDie.h
    Include/GameLogic/Module/DefaultProductionExitUpdate.h
    Include/GameLogic/Module/DefectorSpecialPower.h
    Include/GameLogic/Module/DeletionUpdate.h
    Include/GameLogic/Module/DeliverPayloadAIUpdate.h
    Include/GameLogic/Module/DemoralizeSpecialPower.h
    Include/GameLogic/Module/DemoTrapUpdate.h
    Include/GameLogic/Module/DeployStyleAIUpdate.h
    Include/GameLogic/Module/DestroyDie.h
    Include/GameLogic/Module/DestroyModule.h
    Include/GameLogic/Module/DieModule.h
    Include/GameLogic/Module/DockUpdate.h
    Include/GameLogic/Module/DozerAIUpdate.h
    Include/GameLogic/Module/DumbProjectileBehavior.h
    Include/GameLogic/Module/DynamicGeometryInfoUpdate.h
    Include/GameLogic/Module/DynamicShroudClearingRangeUpdate.h
    Include/GameLogic/Module/EjectPilotDie.h
    Include/GameLogic/Module/EMPUpdate.h
    Include/GameLogic/Module/EnemyNearUpdate.h
    Include/GameLogic/Module/ExperienceScalarUpgrade.h
    Include/GameLogic/Module/FireOCLAfterWeaponCooldownUpdate.h
    Include/GameLogic/Module/FireSpreadUpdate.h
    Include/GameLogic/Module/FirestormDynamicGeometryInfoUpdate.h
    Include/GameLogic/Module/FireWeaponCollide.h
    Include/GameLogic/Module/FireWeaponPower.h
    Include/GameLogic/Module/FireWeaponUpdate.h
    Include/GameLogic/Module/FireWeaponWhenDamagedBehavior.h
    Include/GameLogic/Module/FireWeaponWhenDeadBehavior.h
    Include/GameLogic/Module/FlammableUpdate.h
    Include/GameLogic/Module/FlightDeckBehavior.h
    Include/GameLogic/Module/FloatUpdate.h
    Include/GameLogic/Module/FXListDie.h
    Include/GameLogic/Module/GarrisonContain.h
    Include/GameLogic/Module/GenerateMinefieldBehavior.h
    Include/GameLogic/Module/GrantScienceUpgrade.h
    Include/GameLogic/Module/GrantStealthBehavior.h
    Include/GameLogic/Module/GrantUpgradeCreate.h
    Include/GameLogic/Module/HackInternetAIUpdate.h
    Include/GameLogic/Module/HealContain.h
    Include/GameLogic/Module/HealCrateCollide.h
    Include/GameLogic/Module/HeightDieUpdate.h
    Include/GameLogic/Module/HelicopterSlowDeathUpdate.h
    Include/GameLogic/Module/HelixContain.h
    Include/GameLogic/Module/HighlanderBody.h
    Include/GameLogic/Module/HijackerUpdate.h
    Include/GameLogic/Module/HiveStructureBody.h
    Include/GameLogic/Module/HordeUpdate.h
    Include/GameLogic/Module/ImmortalBody.h
    Include/GameLogic/Module/InactiveBody.h
    Include/GameLogic/Module/InstantDeathBehavior.h
    Include/GameLogic/Module/InternetHackContain.h
    Include/GameLogic/Module/JetAIUpdate.h
    Include/GameLogic/Module/JetSlowDeathBehavior.h
    Include/GameLogic/Module/KeepObjectDie.h
    Include/GameLogic/Module/LaserUpdate.h
    Include/GameLogic/Module/LifetimeUpdate.h
    Include/GameLogic/Module/LockWeaponCreate.h
    Include/GameLogic/Module/LocomotorSetUpgrade.h
    Include/GameLogic/Module/MaxHealthUpgrade.h
    Include/GameLogic/Module/MinefieldBehavior.h
    Include/GameLogic/Module/MissileAIUpdate.h
    Include/GameLogic/Module/MissileLauncherBuildingUpdate.h
    Include/GameLogic/Module/MobMemberSlavedUpdate.h
    Include/GameLogic/Module/MobNexusContain.h
    Include/GameLogic/Module/ModelConditionUpgrade.h
    Include/GameLogic/Module/MoneyCrateCollide.h
    Include/GameLogic/Module/NeutronBlastBehavior.h
    Include/GameLogic/Module/NeutronMissileSlowDeathUpdate.h
    Include/GameLogic/Module/NeutronMissileUpdate.h
    Include/GameLogic/Module/ObjectCreationUpgrade.h
    Include/GameLogic/Module/ObjectDefectionHelper.h
    Include/GameLogic/Module/ObjectHelper.h
    Include/GameLogic/Module/ObjectRepulsorHelper.h
    Include/GameLogic/Module/ObjectSMCHelper.h
    Include/GameLogic/Module/ObjectWeaponStatusHelper.h
    Include/GameLogic/Module/OCLSpecialPower.h
    Include/GameLogic/Module/OCLUpdate.h
    Include/GameLogic/Module/OpenContain.h
    Include/GameLogic/Module/OverchargeBehavior.h
    Include/GameLogic/Module/OverlordContain.h
    Include/GameLogic/Module/ParachuteContain.h
    Include/GameLogic/Module/ParkingPlaceBehavior.h
    Include/GameLogic/Module/ParticleUplinkCannonUpdate.h
    Include/GameLogic/Module/PassengersFireUpgrade.h
    Include/GameLogic/Module/PhysicsUpdate.h
    Include/GameLogic/Module/PilotFindVehicleUpdate.h
    Include/GameLogic/Module/PointDefenseLaserUpdate.h
    Include/GameLogic/Module/PoisonedBehavior.h
    Include/GameLogic/Module/PowerPlantUpdate.h
    Include/GameLogic/Module/PowerPlantUpgrade.h
    Include/GameLogic/Module/POWTruckAIUpdate.h
    Include/GameLogic/Module/POWTruckBehavior.h
    Include/GameLogic/Module/PreorderCreate.h
    Include/GameLogic/Module/PrisonBehavior.h
    Include/GameLogic/Module/PrisonDockUpdate.h
    Include/GameLogic/Module/ProductionUpdate.h
    Include/GameLogic/Module/ProjectileStreamUpdate.h
    Include/GameLogic/Module/ProneUpdate.h
    Include/GameLogic/Module/PropagandaCenterBehavior.h
    Include/GameLogic/Module/PropagandaTowerBehavior.h
    Include/GameLogic/Module/QueueProductionExitUpdate.h
    Include/GameLogic/Module/RadarUpdate.h
    Include/GameLogic/Module/RadarUpgrade.h
    Include/GameLogic/Module/RadiusDecalUpdate.h
    Include/GameLogic/Module/RailedTransportAIUpdate.h
    Include/GameLogic/Module/RailedTransportContain.h
    Include/GameLogic/Module/RailedTransportDockUpdate.h
    Include/GameLogic/Module/RailroadGuideAIUpdate.h
    Include/GameLogic/Module/RebuildHoleBehavior.h
    Include/GameLogic/Module/RebuildHoleExposeDie.h
    Include/GameLogic/Module/RepairDockUpdate.h
    Include/GameLogic/Module/ReplaceObjectUpgrade.h
    Include/GameLogic/Module/RiderChangeContain.h
    Include/GameLogic/Module/SabotageCommandCenterCrateCollide.h
    Include/GameLogic/Module/SabotageFakeBuildingCrateCollide.h
    Include/GameLogic/Module/SabotageInternetCenterCrateCollide.h
    Include/GameLogic/Module/SabotageMilitaryFactoryCrateCollide.h
    Include/GameLogic/Module/SabotagePowerPlantCrateCollide.h
    Include/GameLogic/Module/SabotageSuperweaponCrateCollide.h
    Include/GameLogic/Module/SabotageSupplyCenterCrateCollide.h
    Include/GameLogic/Module/SabotageSupplyDropzoneCrateCollide.h
    Include/GameLogic/Module/SalvageCrateCollide.h
    Include/GameLogic/Module/ShroudCrateCollide.h
    Include/GameLogic/Module/SlavedUpdate.h
    Include/GameLogic/Module/SlowDeathBehavior.h
    Include/GameLogic/Module/SmartBombTargetHomingUpdate.h
    Include/GameLogic/Module/SpawnBehavior.h
    Include/GameLogic/Module/SpawnPointProductionExitUpdate.h
    Include/GameLogic/Module/SpecialAbility.h
    Include/GameLogic/Module/SpecialAbilityUpdate.h
    Include/GameLogic/Module/SpecialPowerCompletionDie.h
    Include/GameLogic/Module/SpecialPowerCreate.h
    Include/GameLogic/Module/SpecialPowerModule.h
    Include/GameLogic/Module/SpecialPowerUpdateModule.h
    Include/GameLogic/Module/SpectreGunshipDeploymentUpdate.h
    Include/GameLogic/Module/SpectreGunshipUpdate.h
    Include/GameLogic/Module/SpyVisionSpecialPower.h
    Include/GameLogic/Module/SpyVisionUpdate.h
    Include/GameLogic/Module/SquishCollide.h
    Include/GameLogic/Module/StatusBitsUpgrade.h
    Include/GameLogic/Module/StatusDamageHelper.h
    Include/GameLogic/Module/StealthDetectorUpdate.h
    Include/GameLogic/Module/StealthUpdate.h
    Include/GameLogic/Module/StealthUpgrade.h
    Include/GameLogic/Module/StickyBombUpdate.h
    Include/GameLogic/Module/StructureBody.h
    Include/GameLogic/Module/StructureCollapseUpdate.h
    Include/GameLogic/Module/StructureToppleUpdate.h
    Include/GameLogic/Module/SubdualDamageHelper.h
    Include/GameLogic/Module/SubObjectsUpgrade.h
    Include/GameLogic/Module/SupplyCenterCreate.h
    Include/GameLogic/Module/SupplyCenterDockUpdate.h
    Include/GameLogic/Module/SupplyCenterProductionExitUpdate.h
    Include/GameLogic/Module/SupplyTruckAIUpdate.h
    Include/GameLogic/Module/SupplyWarehouseCreate.h
    Include/GameLogic/Module/SupplyWarehouseCripplingBehavior.h
    Include/GameLogic/Module/SupplyWarehouseDockUpdate.h
    Include/GameLogic/Module/TechBuildingBehavior.h
    Include/GameLogic/Module/TempWeaponBonusHelper.h
    Include/GameLogic/Module/TensileFormationUpdate.h
    Include/GameLogic/Module/ToppleUpdate.h
    Include/GameLogic/Module/TransitionDamageFX.h
    Include/GameLogic/Module/TransportAIUpdate.h
    Include/GameLogic/Module/TransportContain.h
    Include/GameLogic/Module/TunnelContain.h
    Include/GameLogic/Module/UndeadBody.h
    Include/GameLogic/Module/UnitCrateCollide.h
    Include/GameLogic/Module/UnpauseSpecialPowerUpgrade.h
    Include/GameLogic/Module/UpdateModule.h
    Include/GameLogic/Module/UpgradeDie.h
    Include/GameLogic/Module/UpgradeModule.h
    Include/GameLogic/Module/VeterancyCrateCollide.h
    Include/GameLogic/Module/VeterancyGainCreate.h
    Include/GameLogic/Module/WanderAIUpdate.h
    Include/GameLogic/Module/WaveGuideUpdate.h
    Include/GameLogic/Module/WeaponBonusUpdate.h
    Include/GameLogic/Module/WeaponBonusUpgrade.h
    Include/GameLogic/Module/WeaponSetUpgrade.h
    Include/GameLogic/Module/WorkerAIUpdate.h
    Include/GameLogic/Object.h
    Include/GameLogic/ObjectCreationList.h
    Include/GameLogic/ObjectIter.h
    Include/GameLogic/ObjectScriptStatusBits.h
    Include/GameLogic/ObjectTypes.h
    Include/GameLogic/PartitionManager.h
    Include/GameLogic/PolygonTrigger.h
    Include/GameLogic/Powers.h
#    Include/GameLogic/RankInfo.h
    Include/GameLogic/ScriptActions.h
    Include/GameLogic/ScriptConditions.h
    Include/GameLogic/ScriptEngine.h
    Include/GameLogic/Scripts.h
    Include/GameLogic/SidesList.h
    Include/GameLogic/Squad.h
    Include/GameLogic/TerrainLogic.h
    Include/GameLogic/TurretAI.h
    Include/GameLogic/VictoryConditions.h
    Include/GameLogic/Weapon.h
    Include/GameLogic/WeaponBonusConditionFlags.h
    Include/GameLogic/WeaponSet.h
    Include/GameLogic/WeaponSetFlags.h
    Include/GameLogic/WeaponSetType.h
    Include/GameLogic/WeaponStatus.h
#    Include/GameNetwork/Connection.h
#    Include/GameNetwork/ConnectionManager.h
#    Include/GameNetwork/DisconnectManager.h
#    Include/GameNetwork/DownloadManager.h
#    Include/GameNetwork/FileTransfer.h
#    Include/GameNetwork/FirewallHelper.h
#    Include/GameNetwork/FrameData.h
#    Include/GameNetwork/FrameDataManager.h
#    Include/GameNetwork/FrameMetrics.h
#    Include/GameNetwork/GameInfo.h
#    Include/GameNetwork/GameMessageParser.h
#    Include/GameNetwork/GameSpy/BuddyDefs.h
#    Include/GameNetwork/GameSpy/BuddyThread.h
#    Include/GameNetwork/GameSpy/GameResultsThread.h
#    Include/GameNetwork/GameSpy/GSConfig.h
#    Include/GameNetwork/GameSpy/LadderDefs.h
#    Include/GameNetwork/GameSpy/LobbyUtils.h
#    Include/GameNetwork/GameSpy/MainMenuUtils.h
#    Include/GameNetwork/GameSpy/PeerDefs.h
#    Include/GameNetwork/GameSpy/PeerDefsImplementation.h
#    Include/GameNetwork/GameSpy/PeerThread.h
#    Include/GameNetwork/GameSpy/PersistentStorageDefs.h
#    Include/GameNetwork/GameSpy/PersistentStorageThread.h
#    Include/GameNetwork/GameSpy/PingThread.h
#    Include/GameNetwork/GameSpy/StagingRoomGameInfo.h
#    Include/GameNetwork/GameSpy/ThreadUtils.h
#    Include/GameNetwork/GameSpyChat.h
#    Include/GameNetwork/GameSpyGameInfo.h
#    Include/GameNetwork/GameSpyGP.h
#    Include/GameNetwork/GameSpyOverlay.h
#    Include/GameNetwork/GameSpyThread.h
    Include/GameNetwork/GUIUtil.h
#    Include/GameNetwork/IPEnumeration.h
#    Include/GameNetwork/LANAPI.h
#    Include/GameNetwork/LANAPICallbacks.h
#    Include/GameNetwork/LANGameInfo.h
#    Include/GameNetwork/LANPlayer.h
#    Include/GameNetwork/NAT.h
#    Include/GameNetwork/NetCommandList.h
#    Include/GameNetwork/NetCommandMsg.h
#    Include/GameNetwork/NetCommandRef.h
#    Include/GameNetwork/NetCommandWrapperList.h
#    Include/GameNetwork/NetPacket.h
#    Include/GameNetwork/NetworkDefs.h
#    Include/GameNetwork/NetworkInterface.h
#    Include/GameNetwork/networkutil.h
#    Include/GameNetwork/RankPointValue.h
#    Include/GameNetwork/Transport.h
#    Include/GameNetwork/udp.h
#    Include/GameNetwork/User.h
#    Include/GameNetwork/WOLBrowser/FEBDispatch.h
#    Include/GameNetwork/WOLBrowser/WebBrowser.h
    Include/Precompiled/PreRTS.h
#    Source/Common/Audio/AudioEventRTS.cpp
#    Source/Common/Audio/AudioRequest.cpp
#    Source/Common/Audio/DynamicAudioEventInfo.cpp
#    Source/Common/Audio/GameAudio.cpp
#    Source/Common/Audio/GameMusic.cpp
#    Source/Common/Audio/GameSounds.cpp
    #Source/Common/Audio/simpleplayer.cpp # unused
    #Source/Common/Audio/urllaunch.cpp # unused
    Source/Common/Bezier/BezFwdIterator.cpp
    Source/Common/Bezier/BezierSegment.cpp
    Source/Common/BitFlags.cpp
    Source/Common/CommandLine.cpp
#    Source/Common/crc.cpp
#    Source/Common/CRCDebug.cpp
    Source/Common/DamageFX.cpp
    Source/Common/Dict.cpp
    Source/Common/DiscreteCircle.cpp
    Source/Common/GameEngine.cpp
    Source/Common/GameLOD.cpp
    Source/Common/GameMain.cpp
    Source/Common/GlobalData.cpp
#    Source/Common/INI/INI.cpp
    Source/Common/INI/INIAiData.cpp
    Source/Common/INI/INIAnimation.cpp
#    Source/Common/INI/INIAudioEventInfo.cpp
    Source/Common/INI/INICommandButton.cpp
    Source/Common/INI/INICommandSet.cpp
    Source/Common/INI/INIControlBarScheme.cpp
    Source/Common/INI/INICrate.cpp
    Source/Common/INI/INIDamageFX.cpp
    Source/Common/INI/INIDrawGroupInfo.cpp
    Source/Common/INI/INIGameData.cpp
    Source/Common/INI/INIMapCache.cpp
    Source/Common/INI/INIMapData.cpp
    Source/Common/INI/INIMappedImage.cpp
#    Source/Common/INI/INIMiscAudio.cpp
    Source/Common/INI/INIModel.cpp
    Source/Common/INI/INIMultiplayer.cpp
    Source/Common/INI/INIObject.cpp
    Source/Common/INI/INIParticleSys.cpp
    Source/Common/INI/INISpecialPower.cpp
    Source/Common/INI/INITerrain.cpp
    Source/Common/INI/INITerrainBridge.cpp
    Source/Common/INI/INITerrainRoad.cpp
    Source/Common/INI/INIUpgrade.cpp
#    Source/Common/INI/INIVideo.cpp
    Source/Common/INI/INIWater.cpp
    Source/Common/INI/INIWeapon.cpp
    Source/Common/INI/INIWebpageURL.cpp
    Source/Common/Language.cpp
    Source/Common/MessageStream.cpp
    Source/Common/MiniLog.cpp
    Source/Common/MultiplayerSettings.cpp
    Source/Common/NameKeyGenerator.cpp
    Source/Common/PartitionSolver.cpp
    Source/Common/PerfTimer.cpp
#    Source/Common/RandomValue.cpp
    Source/Common/Recorder.cpp
#    Source/Common/ReplaySimulation.cpp
#    Source/Common/RTS/AcademyStats.cpp
    Source/Common/RTS/ActionManager.cpp
    Source/Common/RTS/Energy.cpp
    Source/Common/RTS/Handicap.cpp
    Source/Common/RTS/MissionStats.cpp
    Source/Common/RTS/Money.cpp
    Source/Common/RTS/Player.cpp
    Source/Common/RTS/PlayerList.cpp
    Source/Common/RTS/PlayerTemplate.cpp
    Source/Common/RTS/ProductionPrerequisite.cpp
    Source/Common/RTS/ResourceGatheringManager.cpp
    Source/Common/RTS/Science.cpp
    Source/Common/RTS/ScoreKeeper.cpp
    Source/Common/RTS/SpecialPower.cpp
    Source/Common/RTS/Team.cpp
    Source/Common/RTS/TunnelTracker.cpp
    Source/Common/SkirmishBattleHonors.cpp
    Source/Common/StateMachine.cpp
    Source/Common/StatsCollector.cpp
#    Source/Common/System/ArchiveFile.cpp
#    Source/Common/System/ArchiveFileSystem.cpp
#    Source/Common/System/AsciiString.cpp
    Source/Common/System/BuildAssistant.cpp
    Source/Common/System/CriticalSection.cpp
    Source/Common/System/DataChunk.cpp
#    Source/Common/System/Debug.cpp
    Source/Common/System/Directory.cpp
    Source/Common/System/DisabledTypes.cpp
    Source/Common/System/encrypt.cpp
#    Source/Common/System/File.cpp
#    Source/Common/System/FileSystem.cpp
    Source/Common/System/FunctionLexicon.cpp
#    Source/Common/System/GameCommon.cpp
    #Source/Common/System/GameMemory.cpp
#    Source/Common/System/GameType.cpp
    Source/Common/System/Geometry.cpp
    Source/Common/System/KindOf.cpp
    Source/Common/System/List.cpp
#    Source/Common/System/LocalFile.cpp
#    Source/Common/System/LocalFileSystem.cpp
    #Source/Common/System/MemoryInit.cpp
#    Source/Common/System/ObjectStatusTypes.cpp
    Source/Common/System/QuotedPrintable.cpp
#    Source/Common/System/Radar.cpp
#    Source/Common/System/RAMFile.cpp
    Source/Common/System/registry.cpp
    Source/Common/System/SaveGame/GameState.cpp
    Source/Common/System/SaveGame/GameStateMap.cpp
#    Source/Common/System/Snapshot.cpp
    Source/Common/System/StackDump.cpp
#    Source/Common/System/StreamingArchiveFile.cpp
#    Source/Common/System/SubsystemInterface.cpp
    Source/Common/System/Trig.cpp
#    Source/Common/System/UnicodeString.cpp
    Source/Common/System/Upgrade.cpp
#    Source/Common/System/Xfer.cpp
#    Source/Common/System/XferCRC.cpp
#    Source/Common/System/XferLoad.cpp
#    Source/Common/System/XferSave.cpp
    Source/Common/TerrainTypes.cpp
    Source/Common/Thing/DrawModule.cpp
    Source/Common/Thing/Module.cpp
    Source/Common/Thing/ModuleFactory.cpp
    Source/Common/Thing/Thing.cpp
    Source/Common/Thing/ThingFactory.cpp
    Source/Common/Thing/ThingTemplate.cpp
#    Source/Common/UserPreferences.cpp
    Source/Common/version.cpp
#    Source/Common/WorkerProcess.cpp
#    Source/GameClient/ClientInstance.cpp
#    Source/GameClient/Color.cpp
#    Source/GameClient/Credits.cpp
#    Source/GameClient/Display.cpp
#    Source/GameClient/DisplayString.cpp
#    Source/GameClient/DisplayStringManager.cpp
    Source/GameClient/Drawable.cpp
#    Source/GameClient/Drawable/Update/AnimatedParticleSysBoneClientUpdate.cpp
#    Source/GameClient/Drawable/Update/BeaconClientUpdate.cpp
#    Source/GameClient/Drawable/Update/SwayClientUpdate.cpp
#    Source/GameClient/DrawGroupInfo.cpp
    Source/GameClient/Eva.cpp
#    Source/GameClient/FXList.cpp
    Source/GameClient/GameClient.cpp
#    Source/GameClient/GameClientDispatch.cpp
#    Source/GameClient/GameText.cpp
#    Source/GameClient/GlobalLanguage.cpp
#    Source/GameClient/GraphDraw.cpp
    Source/GameClient/GUI/AnimateWindowManager.cpp
#    Source/GameClient/GUI/ChallengeGenerals.cpp
#    Source/GameClient/GUI/ControlBar/ControlBar.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarBeacon.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarCommand.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarCommandProcessing.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarMultiSelect.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarObserver.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarOCLTimer.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarPrintPositions.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarResizer.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarScheme.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarStructureInventory.cpp
#    Source/GameClient/GUI/ControlBar/ControlBarUnderConstruction.cpp
    Source/GameClient/GUI/DisconnectMenu/DisconnectMenu.cpp
    Source/GameClient/GUI/EstablishConnectionsMenu/EstablishConnectionsMenu.cpp
#    Source/GameClient/GUI/Gadget/GadgetCheckBox.cpp
#    Source/GameClient/GUI/Gadget/GadgetComboBox.cpp
#    Source/GameClient/GUI/Gadget/GadgetHorizontalSlider.cpp
#    Source/GameClient/GUI/Gadget/GadgetListBox.cpp
#    Source/GameClient/GUI/Gadget/GadgetProgressBar.cpp
#    Source/GameClient/GUI/Gadget/GadgetPushButton.cpp
#    Source/GameClient/GUI/Gadget/GadgetRadioButton.cpp
#    Source/GameClient/GUI/Gadget/GadgetStaticText.cpp
#    Source/GameClient/GUI/Gadget/GadgetTabControl.cpp
#    Source/GameClient/GUI/Gadget/GadgetTextEntry.cpp
#    Source/GameClient/GUI/Gadget/GadgetVerticalSlider.cpp
#    Source/GameClient/GUI/GameFont.cpp
#    Source/GameClient/GUI/GameWindow.cpp
#    Source/GameClient/GUI/GameWindowGlobal.cpp
    Source/GameClient/GUI/GameWindowManager.cpp
    Source/GameClient/GUI/GameWindowManagerScript.cpp
#    Source/GameClient/GUI/GameWindowTransitions.cpp
    Source/GameClient/GUI/GameWindowTransitionsStyles.cpp
#    Source/GameClient/GUI/GUICallbacks/ControlBarCallback.cpp
#    Source/GameClient/GUI/GUICallbacks/ControlBarPopupDescription.cpp
    Source/GameClient/GUI/GUICallbacks/Diplomacy.cpp
    Source/GameClient/GUI/GUICallbacks/ExtendedMessageBox.cpp
    Source/GameClient/GUI/GUICallbacks/GeneralsExpPoints.cpp
    Source/GameClient/GUI/GUICallbacks/IMECandidate.cpp
    Source/GameClient/GUI/GUICallbacks/InGameChat.cpp
    Source/GameClient/GUI/GUICallbacks/InGamePopupMessage.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/ChallengeMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/CreditsMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/DifficultySelect.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/DisconnectWindow.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/DownloadMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/EstablishConnectionsWindow.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/ExtrasMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/GameInfoWindow.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/KeyboardOptionsMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/LanGameOptionsMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/LanLobbyMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/LanMapSelectMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/MainMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/MapSelectMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/NetworkDirectConnect.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/OptionsMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupCommunicator.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupHostGame.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupJoinGame.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupLadderSelect.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupPlayerInfo.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupReplay.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/PopupSaveLoad.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/QuitMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/ReplayMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/ScoreScreen.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/SinglePlayerMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/SkirmishGameOptionsMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/SkirmishMapSelectMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLBuddyOverlay.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLCustomScoreScreen.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLGameSetupMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLLadderScreen.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLLobbyMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLLocaleSelectPopup.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLLoginMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLMapSelectMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLMessageWindow.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLQMScoreScreen.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLQuickMatchMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLStatusMenu.cpp
    Source/GameClient/GUI/GUICallbacks/Menus/WOLWelcomeMenu.cpp
    Source/GameClient/GUI/GUICallbacks/MessageBox.cpp
    Source/GameClient/GUI/GUICallbacks/ReplayControls.cpp
#    Source/GameClient/GUI/HeaderTemplate.cpp
#    Source/GameClient/GUI/IMEManager.cpp
#    Source/GameClient/GUI/LoadScreen.cpp
#    Source/GameClient/GUI/ProcessAnimateWindow.cpp
    Source/GameClient/GUI/Shell/Shell.cpp
    Source/GameClient/GUI/Shell/ShellMenuScheme.cpp
#    Source/GameClient/GUI/WindowLayout.cpp
#    Source/GameClient/GUI/WindowVideoManager.cpp
#    Source/GameClient/GUI/WinInstanceData.cpp
    Source/GameClient/InGameUI.cpp
#    Source/GameClient/Input/Keyboard.cpp
#    Source/GameClient/Input/Mouse.cpp
#    Source/GameClient/LanguageFilter.cpp
#    Source/GameClient/Line2D.cpp
#    Source/GameClient/MapUtil.cpp
#    Source/GameClient/MessageStream/CommandXlat.cpp
#    Source/GameClient/MessageStream/GUICommandTranslator.cpp
#    Source/GameClient/MessageStream/HintSpy.cpp
#    Source/GameClient/MessageStream/HotKey.cpp
#    Source/GameClient/MessageStream/LookAtXlat.cpp
#    Source/GameClient/MessageStream/MetaEvent.cpp
#    Source/GameClient/MessageStream/PlaceEventTranslator.cpp
#    Source/GameClient/MessageStream/SelectionXlat.cpp
#    Source/GameClient/MessageStream/WindowXlat.cpp
#    Source/GameClient/ParabolicEase.cpp
#    Source/GameClient/RadiusDecal.cpp
#    Source/GameClient/SelectionInfo.cpp
#    Source/GameClient/Snow.cpp
#    Source/GameClient/Statistics.cpp
#    Source/GameClient/System/Anim2D.cpp
    Source/GameClient/System/CampaignManager.cpp
#    "Source/GameClient/System/Debug Displayers/AudioDebugDisplay.cpp"
#    Source/GameClient/System/DebugDisplay.cpp
#    Source/GameClient/System/Image.cpp
#    Source/GameClient/System/ParticleSys.cpp
#    Source/GameClient/System/RayEffect.cpp
#    Source/GameClient/System/Smudge.cpp
#    Source/GameClient/Terrain/TerrainRoads.cpp
#    Source/GameClient/Terrain/TerrainVisual.cpp
#    Source/GameClient/VideoPlayer.cpp
#    Source/GameClient/VideoStream.cpp
#    Source/GameClient/View.cpp
#    Source/GameClient/Water.cpp
    Source/GameLogic/AI/AI.cpp
    Source/GameLogic/AI/AIDock.cpp
    Source/GameLogic/AI/AIGroup.cpp
    Source/GameLogic/AI/AIGuard.cpp
    Source/GameLogic/AI/AIGuardRetaliate.cpp
#    Source/GameLogic/AI/AIPathfind.cpp
    Source/GameLogic/AI/AIPlayer.cpp
    Source/GameLogic/AI/AISkirmishPlayer.cpp
    Source/GameLogic/AI/AIStates.cpp
    Source/GameLogic/AI/AITNGuard.cpp
    Source/GameLogic/AI/Squad.cpp
    Source/GameLogic/AI/TurretAI.cpp
    Source/GameLogic/Map/PolygonTrigger.cpp
    Source/GameLogic/Map/SidesList.cpp
    Source/GameLogic/Map/TerrainLogic.cpp
    Source/GameLogic/Object/Armor.cpp
    Source/GameLogic/Object/Behavior/AutoHealBehavior.cpp
    Source/GameLogic/Object/Behavior/BattleBusSlowDeathBehavior.cpp
    Source/GameLogic/Object/Behavior/BehaviorModule.cpp
    Source/GameLogic/Object/Behavior/BridgeBehavior.cpp
    Source/GameLogic/Object/Behavior/BridgeScaffoldBehavior.cpp
    Source/GameLogic/Object/Behavior/BridgeTowerBehavior.cpp
    Source/GameLogic/Object/Behavior/BunkerBusterBehavior.cpp
    Source/GameLogic/Object/Behavior/CountermeasuresBehavior.cpp
    Source/GameLogic/Object/Behavior/DumbProjectileBehavior.cpp
    Source/GameLogic/Object/Behavior/FireWeaponWhenDamagedBehavior.cpp
    Source/GameLogic/Object/Behavior/FireWeaponWhenDeadBehavior.cpp
    Source/GameLogic/Object/Behavior/FlightDeckBehavior.cpp
    Source/GameLogic/Object/Behavior/GenerateMinefieldBehavior.cpp
    Source/GameLogic/Object/Behavior/GrantStealthBehavior.cpp
    Source/GameLogic/Object/Behavior/InstantDeathBehavior.cpp
    Source/GameLogic/Object/Behavior/JetSlowDeathBehavior.cpp
    Source/GameLogic/Object/Behavior/MinefieldBehavior.cpp
    Source/GameLogic/Object/Behavior/NeutonBlastBehavior.cpp
    Source/GameLogic/Object/Behavior/OverchargeBehavior.cpp
    Source/GameLogic/Object/Behavior/ParkingPlaceBehavior.cpp
    Source/GameLogic/Object/Behavior/PoisonedBehavior.cpp
    Source/GameLogic/Object/Behavior/POWTruckBehavior.cpp
    Source/GameLogic/Object/Behavior/PrisonBehavior.cpp
    Source/GameLogic/Object/Behavior/PropagandaCenterBehavior.cpp
    Source/GameLogic/Object/Behavior/PropagandaTowerBehavior.cpp
    Source/GameLogic/Object/Behavior/RebuildHoleBehavior.cpp
    Source/GameLogic/Object/Behavior/SlowDeathBehavior.cpp
    Source/GameLogic/Object/Behavior/SpawnBehavior.cpp
    Source/GameLogic/Object/Behavior/SupplyWarehouseCripplingBehavior.cpp
    Source/GameLogic/Object/Behavior/TechBuildingBehavior.cpp
    Source/GameLogic/Object/Body/ActiveBody.cpp
    Source/GameLogic/Object/Body/BodyModule.cpp
    Source/GameLogic/Object/Body/HighlanderBody.cpp
    Source/GameLogic/Object/Body/HiveStructureBody.cpp
    Source/GameLogic/Object/Body/ImmortalBody.cpp
    Source/GameLogic/Object/Body/InactiveBody.cpp
    Source/GameLogic/Object/Body/StructureBody.cpp
    Source/GameLogic/Object/Body/UndeadBody.cpp
    Source/GameLogic/Object/Collide/CollideModule.cpp
    Source/GameLogic/Object/Collide/CrateCollide/ConvertToCarBombCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/ConvertToHijackedVehicleCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/CrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/HealCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/MoneyCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageCommandCenterCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageFakeBuilding.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageInternetCenterCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageMilitaryFactoryCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotagePowerPlantCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageSuperweaponCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageSupplyCenterCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SabotageSupplyDropzoneCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/SalvageCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/ShroudCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/UnitCrateCollide.cpp
    Source/GameLogic/Object/Collide/CrateCollide/VeterancyCrateCollide.cpp
    Source/GameLogic/Object/Collide/FireWeaponCollide.cpp
    Source/GameLogic/Object/Collide/SquishCollide.cpp
    Source/GameLogic/Object/Contain/CaveContain.cpp
    Source/GameLogic/Object/Contain/GarrisonContain.cpp
    Source/GameLogic/Object/Contain/HealContain.cpp
    Source/GameLogic/Object/Contain/HelixContain.cpp
    Source/GameLogic/Object/Contain/InternetHackContain.cpp
    Source/GameLogic/Object/Contain/MobNexusContain.cpp
    Source/GameLogic/Object/Contain/OpenContain.cpp
    Source/GameLogic/Object/Contain/OverlordContain.cpp
    Source/GameLogic/Object/Contain/ParachuteContain.cpp
    Source/GameLogic/Object/Contain/RailedTransportContain.cpp
    Source/GameLogic/Object/Contain/RiderChangeContain.cpp
    Source/GameLogic/Object/Contain/TransportContain.cpp
    Source/GameLogic/Object/Contain/TunnelContain.cpp
    Source/GameLogic/Object/Create/CreateModule.cpp
    Source/GameLogic/Object/Create/GrantUpgradeCreate.cpp
    Source/GameLogic/Object/Create/LockWeaponCreate.cpp
    Source/GameLogic/Object/Create/PreorderCreate.cpp
    Source/GameLogic/Object/Create/SpecialPowerCreate.cpp
    Source/GameLogic/Object/Create/SupplyCenterCreate.cpp
    Source/GameLogic/Object/Create/SupplyWarehouseCreate.cpp
    Source/GameLogic/Object/Create/VeterancyGainCreate.cpp
    Source/GameLogic/Object/Damage/BoneFXDamage.cpp
    Source/GameLogic/Object/Damage/DamageModule.cpp
    Source/GameLogic/Object/Damage/TransitionDamageFX.cpp
    Source/GameLogic/Object/Destroy/DestroyModule.cpp
    Source/GameLogic/Object/Die/CreateCrateDie.cpp
    Source/GameLogic/Object/Die/CreateObjectDie.cpp
    Source/GameLogic/Object/Die/CrushDie.cpp
    Source/GameLogic/Object/Die/DamDie.cpp
    Source/GameLogic/Object/Die/DestroyDie.cpp
    Source/GameLogic/Object/Die/DieModule.cpp
    Source/GameLogic/Object/Die/EjectPilotDie.cpp
    Source/GameLogic/Object/Die/FXListDie.cpp
    Source/GameLogic/Object/Die/KeepObjectDie.cpp
    Source/GameLogic/Object/Die/RebuildHoleExposeDie.cpp
    Source/GameLogic/Object/Die/SpecialPowerCompletionDie.cpp
    Source/GameLogic/Object/Die/UpgradeDie.cpp
    Source/GameLogic/Object/ExperienceTracker.cpp
    Source/GameLogic/Object/FiringTracker.cpp
    Source/GameLogic/Object/GhostObject.cpp
    Source/GameLogic/Object/Helper/ObjectDefectionHelper.cpp
    Source/GameLogic/Object/Helper/ObjectHelper.cpp
    Source/GameLogic/Object/Helper/ObjectRepulsorHelper.cpp
    Source/GameLogic/Object/Helper/ObjectSMCHelper.cpp
    Source/GameLogic/Object/Helper/ObjectWeaponStatusHelper.cpp
    Source/GameLogic/Object/Helper/StatusDamageHelper.cpp
    Source/GameLogic/Object/Helper/SubdualDamageHelper.cpp
    Source/GameLogic/Object/Helper/TempWeaponBonusHelper.cpp
    Source/GameLogic/Object/Locomotor.cpp
    Source/GameLogic/Object/Object.cpp
    Source/GameLogic/Object/ObjectCreationList.cpp
    Source/GameLogic/Object/ObjectTypes.cpp
    Source/GameLogic/Object/PartitionManager.cpp
    Source/GameLogic/Object/SimpleObjectIterator.cpp
    Source/GameLogic/Object/SpecialPower/BaikonurLaunchPower.cpp
    Source/GameLogic/Object/SpecialPower/CashBountyPower.cpp
    Source/GameLogic/Object/SpecialPower/CashHackSpecialPower.cpp
    Source/GameLogic/Object/SpecialPower/CleanupAreaPower.cpp
    Source/GameLogic/Object/SpecialPower/DefectorSpecialPower.cpp
    Source/GameLogic/Object/SpecialPower/DemoralizeSpecialPower.cpp
    Source/GameLogic/Object/SpecialPower/FireWeaponPower.cpp
    Source/GameLogic/Object/SpecialPower/OCLSpecialPower.cpp
    Source/GameLogic/Object/SpecialPower/SpecialAbility.cpp
    Source/GameLogic/Object/SpecialPower/SpecialPowerModule.cpp
    Source/GameLogic/Object/SpecialPower/SpyVisionSpecialPower.cpp
    Source/GameLogic/Object/Update/AIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/AssaultTransportAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/ChinookAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/DeliverPayloadAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/DeployStyleAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/DozerAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/HackInternetAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/JetAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/MissileAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/POWTruckAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/RailedTransportAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/RailroadGuideAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/SupplyTruckAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/TransportAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/WanderAIUpdate.cpp
    Source/GameLogic/Object/Update/AIUpdate/WorkerAIUpdate.cpp
    Source/GameLogic/Object/Update/AnimationSteeringUpdate.cpp
    Source/GameLogic/Object/Update/AssistedTargetingUpdate.cpp
    Source/GameLogic/Object/Update/AutoDepositUpdate.cpp
    Source/GameLogic/Object/Update/AutoFindHealingUpdate.cpp
    Source/GameLogic/Object/Update/BaseRenerateUpdate.cpp
    Source/GameLogic/Object/Update/BattlePlanUpdate.cpp
    Source/GameLogic/Object/Update/BoneFXUpdate.cpp
    Source/GameLogic/Object/Update/CheckpointUpdate.cpp
    Source/GameLogic/Object/Update/CleanupHazardUpdate.cpp
    Source/GameLogic/Object/Update/CommandButtonHuntUpdate.cpp
    Source/GameLogic/Object/Update/DeletionUpdate.cpp
    Source/GameLogic/Object/Update/DemoTrapUpdate.cpp
    Source/GameLogic/Object/Update/DockUpdate/DockUpdate.cpp
    Source/GameLogic/Object/Update/DockUpdate/PrisonDockUpdate.cpp
    Source/GameLogic/Object/Update/DockUpdate/RailedTransportDockUpdate.cpp
    Source/GameLogic/Object/Update/DockUpdate/RepairDockUpdate.cpp
    Source/GameLogic/Object/Update/DockUpdate/SupplyCenterDockUpdate.cpp
    Source/GameLogic/Object/Update/DockUpdate/SupplyWarehouseDockUpdate.cpp
    Source/GameLogic/Object/Update/DynamicGeometryInfoUpdate.cpp
    Source/GameLogic/Object/Update/DynamicShroudClearingRangeUpdate.cpp
    Source/GameLogic/Object/Update/EMPUpdate.cpp
    Source/GameLogic/Object/Update/EnemyNearUpdate.cpp
    Source/GameLogic/Object/Update/FireOCLAfterWeaponCooldownUpdate.cpp
    Source/GameLogic/Object/Update/FireSpreadUpdate.cpp
    Source/GameLogic/Object/Update/FirestormDynamicGeometryInfoUpdate.cpp
    Source/GameLogic/Object/Update/FireWeaponUpdate.cpp
    Source/GameLogic/Object/Update/FlammableUpdate.cpp
    Source/GameLogic/Object/Update/FloatUpdate.cpp
    Source/GameLogic/Object/Update/HeightDieUpdate.cpp
    Source/GameLogic/Object/Update/HelicopterSlowDeathUpdate.cpp
    Source/GameLogic/Object/Update/HijackerUpdate.cpp
    Source/GameLogic/Object/Update/HordeUpdate.cpp
    Source/GameLogic/Object/Update/LaserUpdate.cpp
    Source/GameLogic/Object/Update/LifetimeUpdate.cpp
    Source/GameLogic/Object/Update/MissileLauncherBuildingUpdate.cpp
    Source/GameLogic/Object/Update/MobMemberSlavedUpdate.cpp
    Source/GameLogic/Object/Update/NeutronMissileSlowDeathUpdate.cpp
    Source/GameLogic/Object/Update/NeutronMissileUpdate.cpp
    Source/GameLogic/Object/Update/OCLUpdate.cpp
    Source/GameLogic/Object/Update/ParticleUplinkCannonUpdate.cpp
    Source/GameLogic/Object/Update/PhysicsUpdate.cpp
    Source/GameLogic/Object/Update/PilotFindVehicleUpdate.cpp
    Source/GameLogic/Object/Update/PointDefenseLaserUpdate.cpp
    Source/GameLogic/Object/Update/PowerPlantUpdate.cpp
    Source/GameLogic/Object/Update/ProductionExitUpdate/DefaultProductionExitUpdate.cpp
    Source/GameLogic/Object/Update/ProductionExitUpdate/QueueProductionExitUpdate.cpp
    Source/GameLogic/Object/Update/ProductionExitUpdate/SpawnPointProductionExitUpdate.cpp
    Source/GameLogic/Object/Update/ProductionExitUpdate/SupplyCenterProductionExitUpdate.cpp
    Source/GameLogic/Object/Update/ProductionUpdate.cpp
    Source/GameLogic/Object/Update/ProjectileStreamUpdate.cpp
    Source/GameLogic/Object/Update/ProneUpdate.cpp
    Source/GameLogic/Object/Update/RadarUpdate.cpp
    Source/GameLogic/Object/Update/RadiusDecalUpdate.cpp
    Source/GameLogic/Object/Update/SlavedUpdate.cpp
    Source/GameLogic/Object/Update/SmartBombTargetHomingUpdate.cpp
    Source/GameLogic/Object/Update/SpecialAbilityUpdate.cpp
    Source/GameLogic/Object/Update/SpecialPowerUpdateModule.cpp
    Source/GameLogic/Object/Update/SpectreGunshipDeploymentUpdate.cpp
    Source/GameLogic/Object/Update/SpectreGunshipUpdate.cpp
    Source/GameLogic/Object/Update/SpyVisionUpdate.cpp
    Source/GameLogic/Object/Update/StealthDetectorUpdate.cpp
    Source/GameLogic/Object/Update/StealthUpdate.cpp
    Source/GameLogic/Object/Update/StickyBombUpdate.cpp
    Source/GameLogic/Object/Update/StructureCollapseUpdate.cpp
    Source/GameLogic/Object/Update/StructureToppleUpdate.cpp
    Source/GameLogic/Object/Update/TensileFormationUpdate.cpp
    Source/GameLogic/Object/Update/ToppleUpdate.cpp
    Source/GameLogic/Object/Update/UpdateModule.cpp
    Source/GameLogic/Object/Update/WaveGuideUpdate.cpp
    Source/GameLogic/Object/Update/WeaponBonusUpdate.cpp
    Source/GameLogic/Object/Upgrade/ActiveShroudUpgrade.cpp
    Source/GameLogic/Object/Upgrade/ArmorUpgrade.cpp
    Source/GameLogic/Object/Upgrade/CommandSetUpgrade.cpp
    Source/GameLogic/Object/Upgrade/CostModifierUpgrade.cpp
    Source/GameLogic/Object/Upgrade/ExperienceScalarUpgrade.cpp
    Source/GameLogic/Object/Upgrade/GrantScienceUpgrade.cpp
    Source/GameLogic/Object/Upgrade/LocomotorSetUpgrade.cpp
    Source/GameLogic/Object/Upgrade/MaxHealthUpgrade.cpp
    Source/GameLogic/Object/Upgrade/ModelConditionUpgrade.cpp
    Source/GameLogic/Object/Upgrade/ObjectCreationUpgrade.cpp
    Source/GameLogic/Object/Upgrade/PassengersFireUpgrade.cpp
    Source/GameLogic/Object/Upgrade/PowerPlantUpgrade.cpp
    Source/GameLogic/Object/Upgrade/RadarUpgrade.cpp
    Source/GameLogic/Object/Upgrade/ReplaceObjectUpgrade.cpp
    Source/GameLogic/Object/Upgrade/StatusBitsUpgrade.cpp
    Source/GameLogic/Object/Upgrade/StealthUpgrade.cpp
    Source/GameLogic/Object/Upgrade/SubObjectsUpgrade.cpp
    Source/GameLogic/Object/Upgrade/UnpauseSpecialPowerUpgrade.cpp
    Source/GameLogic/Object/Upgrade/UpgradeModule.cpp
    Source/GameLogic/Object/Upgrade/WeaponBonusUpgrade.cpp
    Source/GameLogic/Object/Upgrade/WeaponSetUpgrade.cpp
    Source/GameLogic/Object/Weapon.cpp
    Source/GameLogic/Object/WeaponSet.cpp
    Source/GameLogic/ScriptEngine/ScriptActions.cpp
    Source/GameLogic/ScriptEngine/ScriptConditions.cpp
    Source/GameLogic/ScriptEngine/ScriptEngine.cpp
    Source/GameLogic/ScriptEngine/Scripts.cpp
    Source/GameLogic/ScriptEngine/VictoryConditions.cpp
#    Source/GameLogic/System/CaveSystem.cpp
#    Source/GameLogic/System/CrateSystem.cpp
#    Source/GameLogic/System/Damage.cpp
    Source/GameLogic/System/GameLogic.cpp
#    Source/GameLogic/System/GameLogicDispatch.cpp
#    Source/GameLogic/System/RankInfo.cpp
#    Source/GameNetwork/Connection.cpp
#    Source/GameNetwork/ConnectionManager.cpp
#    Source/GameNetwork/DisconnectManager.cpp
#    Source/GameNetwork/DownloadManager.cpp
#    Source/GameNetwork/FileTransfer.cpp
#    Source/GameNetwork/FirewallHelper.cpp
#    Source/GameNetwork/FrameData.cpp
#    Source/GameNetwork/FrameDataManager.cpp
#    Source/GameNetwork/FrameMetrics.cpp
#    Source/GameNetwork/GameInfo.cpp
#    Source/GameNetwork/GameMessageParser.cpp
#    #Source/GameNetwork/GameSpyChat.cpp
#    #Source/GameNetwork/GameSpyGameInfo.cpp
#    #Source/GameNetwork/GameSpyGP.cpp
#    Source/GameNetwork/GameSpy/Chat.cpp
#    Source/GameNetwork/GameSpy/GSConfig.cpp
#    Source/GameNetwork/GameSpy/LadderDefs.cpp
#    Source/GameNetwork/GameSpy/LobbyUtils.cpp
#    Source/GameNetwork/GameSpy/MainMenuUtils.cpp
#    Source/GameNetwork/GameSpy/PeerDefs.cpp
#    Source/GameNetwork/GameSpy/StagingRoomGameInfo.cpp
#    Source/GameNetwork/GameSpy/Thread/BuddyThread.cpp
#    Source/GameNetwork/GameSpy/Thread/GameResultsThread.cpp
#    Source/GameNetwork/GameSpy/Thread/PeerThread.cpp
#    Source/GameNetwork/GameSpy/Thread/PersistentStorageThread.cpp
#    Source/GameNetwork/GameSpy/Thread/PingThread.cpp
#    Source/GameNetwork/GameSpy/Thread/ThreadUtils.cpp
#    Source/GameNetwork/GameSpyOverlay.cpp
    Source/GameNetwork/GUIUtil.cpp
#    Source/GameNetwork/IPEnumeration.cpp
#    Source/GameNetwork/LANAPI.cpp
#    Source/GameNetwork/LANAPICallbacks.cpp
#    Source/GameNetwork/LANAPIhandlers.cpp
#    Source/GameNetwork/LANGameInfo.cpp
#    Source/GameNetwork/NAT.cpp
#    Source/GameNetwork/NetCommandList.cpp
#    Source/GameNetwork/NetCommandMsg.cpp
#    Source/GameNetwork/NetCommandRef.cpp
#    Source/GameNetwork/NetCommandWrapperList.cpp
#    Source/GameNetwork/NetMessageStream.cpp
#    Source/GameNetwork/NetPacket.cpp
#    Source/GameNetwork/Network.cpp
#    Source/GameNetwork/NetworkUtil.cpp
#    Source/GameNetwork/Transport.cpp
#    Source/GameNetwork/udp.cpp
#    Source/GameNetwork/User.cpp
#    Source/GameNetwork/WOLBrowser/WebBrowser.cpp
    Source/Precompiled/PreRTS.cpp
)

if(RTS_GAMEMEMORY_ENABLE)
    # Uses the original Game Memory implementation.
    list(APPEND GAMEENGINE_SRC
#        Source/Common/System/GameMemory.cpp
#        Source/Common/System/GameMemoryInitDMA_GeneralsMD.inl
#        Source/Common/System/GameMemoryInitPools_GeneralsMD.inl
#        Source/Common/System/MemoryInit.cpp
    )
else()
    # Uses the null implementation when disabled.
    list(APPEND GAMEENGINE_SRC
#        Source/Common/System/GameMemoryNull.cpp
#        Include/Common/GameMemoryNull.h
    )
endif()


add_library(z_gameengine STATIC)

target_sources(z_gameengine PRIVATE ${GAMEENGINE_SRC})

target_include_directories(z_gameengine PUBLIC
    Include
    ../CompatLib/Include  # Needed for d3dx8math.h in PCH
)

# Ensure DXVK headers are available when building on Linux
if(NOT SAGE_USE_DX8 AND dxvk_SOURCE_DIR)
    target_include_directories(z_gameengine PUBLIC
        ${dxvk_SOURCE_DIR}/usr/include
        ${dxvk_SOURCE_DIR}/usr/include/dxvk
    )
endif()

target_include_directories(z_gameengine PRIVATE
    Include/Precompiled
)

target_link_libraries(z_gameengine PRIVATE
    corei_gameengine_private
    zi_always
)

# UpdateChecker is now in Core/GameEngine (shared between Generals and GeneralsMD).

target_link_libraries(z_gameengine PUBLIC
    corei_gameengine_public
    z_wwvegas
    d3d8lib  # Ensure DirectX8 headers available (Windows SDK or DXVK)
)

target_precompile_headers(z_gameengine PRIVATE
    [["Utility/CppMacros.h"]] # Must be first, to be removed when abandoning VC6
    Include/Precompiled/PreRTS.h
)
