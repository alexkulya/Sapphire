#include <Common.h>
#include <Network/CommonNetwork.h>
#include <Network/GamePacketNew.h>
#include <Logging/Logger.h>
#include <Network/PacketContainer.h>
#include <Network/CommonActorControl.h>
#include <Network/PacketDef/Zone/ClientZoneDef.h>
#include <Exd/ExdDataGenerated.h>

#include <unordered_map>

#include "Network/GameConnection.h"

#include "Session.h"

#include "Manager/TerritoryMgr.h"
#include "Territory/Zone.h"
#include "Territory/InstanceContent.h"

#include "Network/PacketWrappers/InitUIPacket.h"
#include "Network/PacketWrappers/PingPacket.h"
#include "Network/PacketWrappers/MoveActorPacket.h"
#include "Network/PacketWrappers/ChatPacket.h"
#include "Network/PacketWrappers/ServerNoticePacket.h"
#include "Network/PacketWrappers/ActorControlPacket142.h"
#include "Network/PacketWrappers/ActorControlPacket143.h"
#include "Network/PacketWrappers/ActorControlPacket144.h"
#include "Network/PacketWrappers/EventStartPacket.h"
#include "Network/PacketWrappers/EventFinishPacket.h"
#include "Network/PacketWrappers/PlayerStateFlagsPacket.h"

#include "ServerMgr.h"
#include "Framework.h"

using namespace Sapphire::Common;
using namespace Sapphire::Network::Packets;
using namespace Sapphire::Network::Packets::Server;
using namespace Sapphire::Network::ActorControl;
using namespace Sapphire::World::Manager;

enum GmCommand
{
  Pos = 0x0000,
  Lv = 0x0001,
  Race = 0x0002,
  Tribe = 0x0003,
  Sex = 0x0004,
  Time = 0x0005,
  Weather = 0x0006,
  Call = 0x0007,
  Inspect = 0x0008,
  Speed = 0x0009,
  Invis = 0x000D,

  Raise = 0x0010,
  Kill = 0x000E,
  Icon = 0x0012,

  Hp = 0x0064,
  Mp = 0x0065,
  Tp = 0x0066,
  Gp = 0x0067,
  Exp = 0x0068,
  Inv = 0x006A,

  Orchestrion = 0x0074,

  Item = 0x00C8,
  Gil = 0x00C9,
  Collect = 0x00CA,

  QuestAccept = 0x012C,
  QuestCancel = 0x012D,
  QuestComplete = 0x012E,
  QuestIncomplete = 0x012F,
  QuestSequence = 0x0130,
  QuestInspect = 0x0131,
  GC = 0x0154,
  GCRank = 0x0155,
  Aetheryte = 0x015E,
  Wireframe = 0x0226,
  Teri = 0x0258,
  Kick = 0x025C,
  TeriInfo = 0x025D,
  Jump = 0x025E,
  JumpNpc = 0x025F,
};

void Sapphire::Network::GameConnection::gm1Handler( FrameworkPtr pFw,
                                                    const Packets::FFXIVARR_PACKET_RAW& inPacket,
                                                    Entity::Player& player )
{
  if( player.getGmRank() <= 0 )
    return;

  const auto packet = ZoneChannelPacket< Client::FFXIVIpcGmCommand1 >( inPacket );
  const auto commandId = packet.data().commandId;
  const auto param1 = packet.data().param1;
  const auto param2 = packet.data().param2;
  const auto param3 = packet.data().param3;
  const auto param4 = packet.data().param4;
  const auto target = packet.data().target;

  Logger::info( "{0} used GM1 commandId: {1}, params: {2}, {3}, {4}, {5}, target: {6}",
                player.getName(), commandId,
                param1, param2, param3, param4, target );

  Sapphire::Entity::ActorPtr targetActor;


  if( player.getId() == target )
  {
    targetActor = player.getAsPlayer();
  }
  else
  {
    auto inRange = player.getInRangeActors();
    for( auto& actor : inRange )
    {
      if( actor->getId() == target )
        targetActor = actor;
    }
  }

  if( !targetActor )
    return;
  auto targetPlayer = targetActor->getAsPlayer();

  switch( commandId )
  {
    case GmCommand::Lv:
    {
      targetPlayer->setLevel( param1 );
      player.sendNotice( "Level for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      break;
    }
    case GmCommand::Race:
    {
      targetPlayer->setLookAt( CharaLook::Race, param1 );
      player.sendNotice( "Race for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      targetPlayer->spawn( targetPlayer );
      auto inRange = targetPlayer->getInRangeActors();
      for( auto actor : inRange )
      {
        targetPlayer->despawn( actor->getAsPlayer() );
        targetPlayer->spawn( actor->getAsPlayer() );
      }
      break;
    }
    case GmCommand::Tribe:
    {
      targetPlayer->setLookAt( CharaLook::Tribe, param1 );
      player.sendNotice( "Tribe for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      targetPlayer->spawn( targetPlayer );
      auto inRange = targetPlayer->getInRangeActors();
      for( auto actor : inRange )
      {
        targetPlayer->despawn( actor->getAsPlayer() );
        targetPlayer->spawn( actor->getAsPlayer() );
      }
      break;
    }
    case GmCommand::Sex:
    {
      targetPlayer->setLookAt( CharaLook::Gender, param1 );
      player.sendNotice( "Sex for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      targetPlayer->spawn( targetPlayer );
      auto inRange = targetActor->getInRangeActors();
      for( auto actor : inRange )
      {
        targetPlayer->despawn( actor->getAsPlayer() );
        targetPlayer->spawn( actor->getAsPlayer() );
      }
      break;
    }
    case GmCommand::Time:
    {
      player.setEorzeaTimeOffset( param2 );
      player.sendNotice( "Eorzea time offset: " + std::to_string( param2 ) );
      break;
    }
    case GmCommand::Weather:
    {
      targetPlayer->getCurrentZone()->setWeatherOverride( static_cast< Common::Weather >( param1 ) );
      player.sendNotice( "Weather in Zone \"" + targetPlayer->getCurrentZone()->getName() + "\" of " +
                         targetPlayer->getName() + " set in range." );
      break;
    }
    case GmCommand::Call:
    {
      if( targetPlayer->getZoneId() != player.getZoneId() )
        targetPlayer->setZone( player.getZoneId() );

      targetPlayer->changePosition( player.getPos().x, player.getPos().y, player.getPos().z, player.getRot() );
      player.sendNotice( "Calling " + targetPlayer->getName() );
      break;
    }
    case GmCommand::Inspect:
    {
      player.sendNotice( "Name: " + targetPlayer->getName() +
                         "\nGil: " + std::to_string( targetPlayer->getCurrency( CurrencyType::Gil ) ) +
                         "\nZone: " + targetPlayer->getCurrentZone()->getName() +
                         "(" + std::to_string( targetPlayer->getZoneId() ) + ")" +
                         "\nClass: " + std::to_string( static_cast< uint8_t >( targetPlayer->getClass() ) ) +
                         "\nLevel: " + std::to_string( targetPlayer->getLevel() ) +
                         "\nExp: " + std::to_string( targetPlayer->getExp() ) +
                         "\nSearchMessage: " + targetPlayer->getSearchMessage() +
                         "\nPlayTime: " + std::to_string( targetPlayer->getPlayTime() ) );
      break;
    }
    case GmCommand::Speed:
    {
      targetPlayer->queuePacket( makeActorControl143( player.getId(), Flee, param1 ) );
      player.sendNotice( "Speed for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      break;
    }
    case GmCommand::Invis:
    {
      player.setGmInvis( !player.getGmInvis() );
      player.sendNotice( "Invisibility flag for " + player.getName() +
                         " was toggled to " + std::to_string( !player.getGmInvis() ) );

      for( auto actor : player.getInRangeActors() )
      {
        player.despawn( actor->getAsPlayer() );
        player.spawn( actor->getAsPlayer() );
      }
      break;
    }
    case GmCommand::Kill:
    {
      targetActor->getAsChara()->takeDamage( 9999999 );
      player.sendNotice( "Killed " + std::to_string( targetActor->getId() ) );
      break;
    }
    case GmCommand::Icon:
    {
      targetPlayer->setOnlineStatusMask( param1 );

      auto statusPacket = makeZonePacket< FFXIVIpcSetOnlineStatus >( player.getId() );
      statusPacket->data().onlineStatusFlags = param1;
      queueOutPacket( statusPacket );

      auto searchInfoPacket = makeZonePacket< FFXIVIpcSetSearchInfo >( player.getId() );
      searchInfoPacket->data().onlineStatusFlags = param1;
      searchInfoPacket->data().selectRegion = targetPlayer->getSearchSelectRegion();
      strcpy( searchInfoPacket->data().searchMessage, targetPlayer->getSearchMessage() );
      targetPlayer->queuePacket( searchInfoPacket );

      targetPlayer->sendToInRangeSet( makeActorControl142( player.getId(), SetStatusIcon,
                                                           static_cast< uint8_t >( player.getOnlineStatus() ) ),
                                      true );
      player.sendNotice( "Icon for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      break;
    }
    case GmCommand::Hp:
    {
      targetPlayer->setHp( param1 );
      player.sendNotice( "Hp for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      break;
    }
    case GmCommand::Mp:
    {
      targetPlayer->setMp( param1 );
      player.sendNotice( "Mp for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      break;
    }
    case GmCommand::Gp:
    {
      targetPlayer->setHp( param1 );
      player.sendNotice( "Gp for " + targetPlayer->getName() + " was set to " + std::to_string( param1 ) );
      break;
    }
    case GmCommand::Exp:
    {
      targetPlayer->gainExp( param1 );
      player.sendNotice( std::to_string( param1 ) + " Exp was added to " + targetPlayer->getName() );
      break;
    }
    case GmCommand::Inv:
    {
      if( targetActor->getAsChara()->getInvincibilityType() == Common::InvincibilityType::InvincibilityRefill )
        targetActor->getAsChara()->setInvincibilityType( Common::InvincibilityType::InvincibilityNone );
      else
        targetActor->getAsChara()->setInvincibilityType( Common::InvincibilityType::InvincibilityRefill );

      player.sendNotice( "Invincibility for " + targetPlayer->getName() +
                         " was switched." );
      break;
    }
    case GmCommand::Orchestrion:
    {
      if( param1 == 1 )
      {
        if( param2 == 0 )
        {
          for( uint8_t i = 0; i < 255; i++ )
            targetActor->getAsPlayer()->learnSong( i, 0 );

          player.sendNotice( "All Songs for " + targetPlayer->getName() +
                             " were turned on." );
        }
        else
        {
          targetActor->getAsPlayer()->learnSong( param2, 0 );
          player.sendNotice( "Song " + std::to_string( param2 ) + " for " + targetPlayer->getName() +
                             " was turned on." );
        }
      }

      break;
    }
    case GmCommand::Item:
    {
      auto quantity = param2;

      if( quantity < 1 || quantity > 999 )
      {
        quantity = 1;
      }

      if( ( param1 == 0xcccccccc ) )
      {
        player.sendUrgent( "Syntaxerror." );
        return;
      }

      if( !targetPlayer->addItem( param1, quantity ) )
        player.sendUrgent( "Item " + std::to_string( param1 ) + " could not be added to inventory." );
      break;
    }
    case GmCommand::Gil:
    {
      targetPlayer->addCurrency( CurrencyType::Gil, param1 );
      player.sendNotice( "Added " + std::to_string( param1 ) + " Gil for " + targetPlayer->getName() );
      break;
    }
    case GmCommand::Collect:
    {
      uint32_t gil = targetPlayer->getCurrency( CurrencyType::Gil );

      if( gil < param1 )
      {
        player.sendUrgent( "Player does not have enough Gil(" + std::to_string( gil ) + ")" );
      }
      else
      {
        targetPlayer->removeCurrency( CurrencyType::Gil, param1 );
        player.sendNotice( "Removed " + std::to_string( param1 ) +
                           " Gil from " + targetPlayer->getName() +
                           "(" + std::to_string( gil ) + " before)" );
      }
      break;
    }
    case GmCommand::QuestAccept:
    {
      targetPlayer->updateQuest( param1, 1 );
      break;
    }
    case GmCommand::QuestCancel:
    {
      targetPlayer->removeQuest( param1 );
      break;
    }
    case GmCommand::QuestComplete:
    {
      targetPlayer->finishQuest( param1 );
      break;
    }
    case GmCommand::QuestIncomplete:
    {
      targetPlayer->unfinishQuest( param1 );
      break;
    }
    case GmCommand::QuestSequence:
    {
      targetPlayer->updateQuest( param1, param2 );
      break;
    }
    case GmCommand::GC:
    {
      targetPlayer->setGc( param1 );
      player.sendNotice( "GC for " + targetPlayer->getName() +
                         " was set to " + std::to_string( targetPlayer->getGc() ) );
      break;
    }
    case GmCommand::GCRank:
    {
      targetPlayer->setGcRankAt( targetPlayer->getGc() - 1, param1 );
      player.sendNotice( "GC Rank for  " + targetPlayer->getName() +
                         " for GC " + std::to_string( targetPlayer->getGc() ) +
                         " was set to " +
                         std::to_string( targetPlayer->getGcRankArray()[ targetPlayer->getGc() - 1 ] ) );
      break;
    }
    case GmCommand::Aetheryte:
    {
      if( param1 == 0 )
      {
        if( param2 == 0 )
        {
          for( uint8_t i = 0; i < 255; i++ )
            targetActor->getAsPlayer()->registerAetheryte( i );

          player.sendNotice( "All Aetherytes for " + targetPlayer->getName() +
                             " were turned on." );
        }
        else
        {
          targetActor->getAsPlayer()->registerAetheryte( param2 );
          player.sendNotice( "Aetheryte " + std::to_string( param2 ) + " for " + targetPlayer->getName() +
                             " was turned on." );
        }
      }

      break;
    }
    case GmCommand::Wireframe:
    {
      player.queuePacket(
        std::make_shared< ActorControlPacket143 >( player.getId(), ActorControlType::ToggleWireframeRendering ) );
      player.sendNotice( "Wireframe Rendering for " + player.getName() + " was toggled" );
      break;
    }
    case GmCommand::Teri:
    {
      auto pTeriMgr = pFw->get< TerritoryMgr >();
      if( auto instance = pTeriMgr->getInstanceZonePtr( param1 ) )
      {
        player.sendDebug( "Found instance: " + instance->getName() + ", id: " + std::to_string( param1 ) );

        // if the zone is an instanceContent instance, make sure the player is actually bound to it
        auto pInstance = instance->getAsInstanceContent();

        // pInstance will be nullptr if you're accessing a normal zone via its allocated instance id rather than its zoneid
        if( pInstance && !pInstance->isPlayerBound( player.getId() ) )
        {
          player.sendUrgent( "Not able to join instance: " + std::to_string( param1 ) );
          player.sendUrgent(
            "Player not bound! ( run !instance bind <instanceId> first ) " + std::to_string( param1 ) );
          break;
        }

        player.setInstance( instance );
      }
      else if( !pTeriMgr->isValidTerritory( param1 ) )
      {
        player.sendUrgent( "Invalid zone " + std::to_string( param1 ) );
      }
      else
      {
        auto pZone = pTeriMgr->getZoneByTerritoryTypeId( param1 );
        if( !pZone )
        {
          player.sendUrgent( "No zone instance found for " + std::to_string( param1 ) );
          break;
        }

        if( !pTeriMgr->isDefaultTerritory( param1 ) )
        {
          player.sendUrgent( pZone->getName() + " is an instanced area - instance ID required to zone in." );
          break;
        }

        bool doTeleport = false;
        uint16_t teleport;

        auto pExdData = pFw->get< Data::ExdDataGenerated >();
        auto idList = pExdData->getAetheryteIdList();

        for( auto i : idList )
        {
          auto data = pExdData->get< Sapphire::Data::Aetheryte >( i );

          if( !data )
          {
            continue;
          }

          if( data->territory == param1 )
          {
            if( data->isAetheryte )
            {
              doTeleport = true;
              teleport = i;
              break;
            }
          }
        }
        if( doTeleport )
        {
          player.teleport( teleport );
        }
        else
        {
          targetPlayer->setPos( targetPlayer->getPos() );
          targetPlayer->performZoning( param1, targetPlayer->getPos(), 0 );
        }

        player.sendNotice( targetPlayer->getName() + " was warped to zone " +
                           std::to_string( param1 ) + " (" + pZone->getName() + ")" );
      }
      break;
    }
    case GmCommand::Kick:
    {
      // todo: this doesn't kill their session straight away, should do this properly but its good for when you get stuck for now
      targetPlayer->setMarkedForRemoval();

      player.sendNotice( "Kicked " + targetPlayer->getName() );

      break;
    }
    case GmCommand::TeriInfo:
    {
      auto pCurrentZone = player.getCurrentZone();
      player.sendNotice( "ZoneId: " + std::to_string( player.getZoneId() ) +
                         "\nName: " + pCurrentZone->getName() +
                         "\nInternalName: " + pCurrentZone->getInternalName() +
                         "\nGuId: " + std::to_string( pCurrentZone->getGuId() ) +
                         "\nPopCount: " + std::to_string( pCurrentZone->getPopCount() ) +
                         "\nCurrentWeather: " +
                         std::to_string( static_cast< uint8_t >( pCurrentZone->getCurrentWeather() ) ) +
                         "\nNextWeather: " +
                         std::to_string( static_cast< uint8_t >( pCurrentZone->getNextWeather() ) ) );
      break;
    }
    case GmCommand::Jump:
    {

      auto inRange = player.getInRangeActors();

      player.changePosition( targetActor->getPos().x, targetActor->getPos().y, targetActor->getPos().z,
                             targetActor->getRot() );

      player.sendNotice( "Jumping to " + targetPlayer->getName() + " in range." );
      break;
    }

    default:
      player.sendUrgent( "GM1 Command not implemented: " + std::to_string( commandId ) );
      break;
  }

}

void Sapphire::Network::GameConnection::gm2Handler( FrameworkPtr pFw,
                                                    const Packets::FFXIVARR_PACKET_RAW& inPacket,
                                                    Entity::Player& player )
{
  if( player.getGmRank() <= 0 )
    return;

  auto pServerZone = pFw->get< World::ServerMgr >();

  const auto packet = ZoneChannelPacket< Client::FFXIVIpcGmCommand2 >( inPacket );

  const auto commandId = packet.data().commandId;
  const auto param1 = packet.data().param1;
  const auto param2 = packet.data().param2;
  const auto param3 = packet.data().param3;
  const auto param4 = packet.data().param4;
  const auto target = std::string( packet.data().target );

  Logger::debug( "{0} used GM2 commandId: {1}, params: {2}, {3}, {4}, {5}, target: {6}",
                 player.getName(), commandId, param1, param2, param3, param4, target );

  auto targetSession = pServerZone->getSession( target );
  Sapphire::Entity::CharaPtr targetActor;

  if( targetSession != nullptr )
  {
    targetActor = targetSession->getPlayer();
  }
  else
  {
    if( target == "self" )
    {
      targetActor = player.getAsPlayer();
    }
    else
    {
      player.sendUrgent( "Player " + target + " not found on this server." );
      return;
    }
  }

  if( !targetActor )
    return;

  auto targetPlayer = targetActor->getAsPlayer();

  switch( commandId )
  {
    case GmCommand::Raise:
    {
      targetPlayer->resetHp();
      targetPlayer->resetMp();
      targetPlayer->setStatus( Common::ActorStatus::Idle );
      targetPlayer->sendZoneInPackets( 0x01, 0x01, 0, 113, true );


      targetPlayer->sendToInRangeSet( makeActorControl143( player.getId(), ZoneIn, 0x01, 0x01, 0, 113 ), true );
      targetPlayer->sendToInRangeSet( makeActorControl142( player.getId(), SetStatus,
                                                           static_cast< uint8_t >( Common::ActorStatus::Idle ) ),
                                      true );
      player.sendNotice( "Raised " + targetPlayer->getName() );
      break;
    }
    case GmCommand::Jump:
    {
      if( targetPlayer->getZoneId() != player.getZoneId() )
      {
        player.setZone( targetPlayer->getZoneId() );
      }
      player.changePosition( targetActor->getPos().x, targetActor->getPos().y, targetActor->getPos().z,
                             targetActor->getRot() );
      player.sendNotice( "Jumping to " + targetPlayer->getName() );
      break;
    }
    case GmCommand::Call:
    {
      // We shouldn't be able to call a player into an instance, only call them out of one
      if( player.getCurrentInstance() )
      {
        player.sendUrgent( "You are unable to call a player while bound to a battle instance." );
        return;
      }

      targetPlayer->setInstance( player.getCurrentZone() );

      targetPlayer->changePosition( player.getPos().x, player.getPos().y, player.getPos().z, player.getRot() );
      player.sendNotice( "Calling " + targetPlayer->getName() );
      break;
    }
    default:
      player.sendUrgent( "GM2 Command not implemented: " + std::to_string( commandId ) );
      break;
  }

}
