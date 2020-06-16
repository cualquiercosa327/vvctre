// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Network {

constexpr u32 network_version = 4; ///< The version of this Room and RoomMember

constexpr u16 DefaultRoomPort = 24872;

constexpr u32 MaxMessageSize = 500;

/// Maximum number of concurrent connections allowed to this room.
static constexpr u32 MaxConcurrentConnections = 254;

constexpr std::size_t NumChannels = 1; // Number of channels used for the connection

struct RoomInformation {
    std::string name;           ///< Name of the server
    std::string description;    ///< Server description
    u32 member_slots;           ///< Maximum number of members in this room
    u16 port;                   ///< The port of this room
    std::string preferred_game; ///< Game to advertise that you want to play
    u64 preferred_game_id;      ///< Title ID for the advertised game
};

struct GameInfo {
    std::string name;
    u64 id = 0;
};

using MacAddress = std::array<u8, 6>;

/// A special MAC address that tells the room we're joining to assign us a MAC address
/// automatically.
constexpr MacAddress NoPreferredMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 802.11 broadcast MAC address
constexpr MacAddress BroadcastMac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/// Information about the received WiFi packets.
/// Acts as our own 802.11 header.
struct WifiPacket {
    enum class PacketType : u8 {
        Beacon,
        Data,
        Authentication,
        AssociationResponse,
        Deauthentication,
        NodeMap
    };
    PacketType type;      ///< The type of 802.11 frame.
    std::vector<u8> data; ///< Raw 802.11 frame data, starting at the management frame header
                          /// for management frames.
    MacAddress transmitter_address; ///< Mac address of the transmitter.
    MacAddress destination_address; ///< Mac address of the receiver.
    u8 channel;                     ///< WiFi channel where this frame was transmitted.
};

/// Represents a chat message.
struct ChatEntry {
    std::string nickname;
    std::string message;
};

// The different types of messages that can be sent. The first byte of each packet defines the type
enum RoomMessageTypes : u8 {
    IdJoinRequest = 1,
    IdJoinSuccess,
    IdRoomInformation,
    IdSetGameInfo,
    IdWifiPacket,
    IdChatMessage,
    IdNameCollision,
    IdMacCollision,
    IdVersionMismatch,
    IdWrongPassword,
    IdCloseRoom,
    IdRoomIsFull,
    IdConsoleIdCollision,
    IdStatusMessage,
    IdHostKicked,
    IdHostBanned,
};

/// Types of system status messages
enum StatusMessageTypes : u8 {
    IdMemberJoin = 1,  ///< Member joining
    IdMemberLeave,     ///< Member leaving
    IdMemberKicked,    ///< A member is kicked from the room
    IdMemberBanned,    ///< A member is banned from the room
    IdAddressUnbanned, ///< Someone is unbanned from the room
};

/// Represents a system status message.
struct StatusMessageEntry {
    StatusMessageTypes type;
    std::string nickname;
};

/**
 * This is what a client [person joining a server] would use.
 */
class RoomMember final {
public:
    enum class State : u8 {
        Uninitialized, ///< Not initialized
        Idle,          ///< Default state (i.e. not connected)
        Joining,       ///< The client is attempting to join a room.
        Joined, ///< The client is connected to the room and is ready to send/receive packets.
    };

    enum class Error : u8 {
        // Reasons why connection was closed
        LostConnection, ///< Connection closed
        HostKicked,     ///< Kicked by the host

        // Reasons why connection was rejected
        UnknownError,       ///< Some error [permissions to network device missing or something]
        NameCollision,      ///< Somebody is already using this name
        MacCollision,       ///< Somebody is already using that mac-address
        ConsoleIdCollision, ///< Somebody in the room has the same Console ID
        WrongVersion,       ///< The room version is not the same as for this RoomMember
        WrongPassword,      ///< The password doesn't match the one from the Room
        CouldNotConnect,    ///< The room is not responding to a connection attempt
        RoomIsFull,         ///< Room is already at the maximum number of players
        HostBanned,         ///< The user is banned by the host
    };

    struct MemberInformation {
        std::string nickname;   ///< Nickname of the member.
        std::string unused1;    ///< unused
        std::string unused2;    ///< unused
        std::string unused3;    ///< unused
        GameInfo game_info;     ///< Name of the game they're currently playing, or empty if they're
                                /// not playing anything.
        MacAddress mac_address; ///< MAC address associated with this member.
    };
    using MemberList = std::vector<MemberInformation>;

    // The handle for the callback functions
    template <typename T>
    using CallbackHandle = std::shared_ptr<std::function<void(const T&)>>;

    /**
     * Unbinds a callback function from the events.
     * @param handle The connection handle to disconnect
     */
    template <typename T>
    void Unbind(CallbackHandle<T> handle);

    RoomMember();
    ~RoomMember();

    /**
     * Returns the status of our connection to the room.
     */
    State GetState() const;

    /**
     * Returns information about the members in the room we're currently connected to.
     */
    const MemberList& GetMemberInformation() const;

    /**
     * Returns the nickname of the RoomMember.
     */
    const std::string& GetNickname() const;

    /**
     * Returns the username of the RoomMember.
     */
    const std::string& GetUsername() const;

    /**
     * Returns the MAC address of the RoomMember.
     */
    const MacAddress& GetMacAddress() const;

    /**
     * Returns information about the room we're currently connected to.
     */
    RoomInformation GetRoomInformation() const;

    /**
     * Returns whether we're connected to a server or not.
     */
    bool IsConnected() const;

    /**
     * Attempts to join a room at the specified address and port, using the specified nickname.
     * A console ID hash is passed in to check console ID conflicts.
     * This may fail if the username or console ID is already taken.
     */
    void Join(const std::string& nickname, const std::string& console_id_hash,
              const char* server_addr = "127.0.0.1", u16 server_port = DefaultRoomPort,
              const MacAddress& preferred_mac = NoPreferredMac, const std::string& password = "");

    /**
     * Sends a WiFi packet to the room.
     * @param packet The WiFi packet to send.
     */
    void SendWifiPacket(const WifiPacket& packet);

    /**
     * Sends a chat message to the room.
     * @param message The contents of the message.
     */
    void SendChatMessage(const std::string& message);

    /**
     * Sends the current game info to the room.
     * @param game_info The game information.
     */
    void SendGameInfo(const GameInfo& game_info);

    /**
     * Sends a moderation request to the room.
     * @param type Moderation request type.
     * @param nickname The subject of the request. (i.e. the user you want to kick/ban)
     */
    void SendModerationRequest(RoomMessageTypes type, const std::string& nickname);

    /**
     * Attempts to retrieve ban list from the room.
     * If success, the ban list callback would be called. Otherwise an error would be emitted.
     */
    void RequestBanList();

    /**
     * Binds a function to an event that will be triggered every time the State of the member
     * changed. The function wil be called every time the event is triggered. The callback function
     * must not bind or unbind a function. Doing so will cause a deadlock
     * @param callback The function to call
     * @return A handle used for removing the function from the registered list
     */
    CallbackHandle<State> BindOnStateChanged(std::function<void(const State&)> callback);

    /**
     * Binds a function to an event that will be triggered every time an error happened. The
     * function wil be called every time the event is triggered. The callback function must not bind
     * or unbind a function. Doing so will cause a deadlock
     * @param callback The function to call
     * @return A handle used for removing the function from the registered list
     */
    CallbackHandle<Error> BindOnError(std::function<void(const Error&)> callback);

    /**
     * Binds a function to an event that will be triggered every time a WifiPacket is received.
     * The function wil be called everytime the event is triggered.
     * The callback function must not bind or unbind a function. Doing so will cause a deadlock
     * @param callback The function to call
     * @return A handle used for removing the function from the registered list
     */
    CallbackHandle<WifiPacket> BindOnWifiPacketReceived(
        std::function<void(const WifiPacket&)> callback);

    /**
     * Binds a function to an event that will be triggered every time the RoomInformation changes.
     * The function wil be called every time the event is triggered.
     * The callback function must not bind or unbind a function. Doing so will cause a deadlock
     * @param callback The function to call
     * @return A handle used for removing the function from the registered list
     */
    CallbackHandle<RoomInformation> BindOnRoomInformationChanged(
        std::function<void(const RoomInformation&)> callback);

    /**
     * Binds a function to an event that will be triggered every time a ChatMessage is received.
     * The function wil be called every time the event is triggered.
     * The callback function must not bind or unbind a function. Doing so will cause a deadlock
     * @param callback The function to call
     * @return A handle used for removing the function from the registered list
     */
    CallbackHandle<ChatEntry> BindOnChatMessageReceived(
        std::function<void(const ChatEntry&)> callback);

    /**
     * Binds a function to an event that will be triggered every time a StatusMessage is
     * received. The function will be called every time the event is triggered. The callback
     * function must not bind or unbind a function. Doing so will cause a deadlock
     * @param callback The function to call
     * @return A handle used for removing the function from the registered list
     */
    CallbackHandle<StatusMessageEntry> BindOnStatusMessageReceived(
        std::function<void(const StatusMessageEntry&)> callback);

    /**
     * Leaves the current room.
     */
    void Leave();

private:
    class RoomMemberImpl;
    std::unique_ptr<RoomMemberImpl> room_member_impl;
};

inline const char* GetStateStr(const RoomMember::State& s) {
    switch (s) {
    case RoomMember::State::Uninitialized:
        return "Uninitialized";
    case RoomMember::State::Idle:
        return "Idle";
    case RoomMember::State::Joining:
        return "Joining";
    case RoomMember::State::Joined:
        return "Joined";
    }
    return "Unknown";
}

inline const char* GetErrorStr(const RoomMember::Error& e) {
    switch (e) {
    case RoomMember::Error::LostConnection:
        return "LostConnection";
    case RoomMember::Error::HostKicked:
        return "HostKicked";
    case RoomMember::Error::UnknownError:
        return "UnknownError";
    case RoomMember::Error::NameCollision:
        return "NameCollision";
    case RoomMember::Error::MacCollision:
        return "MacCollision";
    case RoomMember::Error::ConsoleIdCollision:
        return "ConsoleIdCollision";
    case RoomMember::Error::WrongVersion:
        return "WrongVersion";
    case RoomMember::Error::WrongPassword:
        return "WrongPassword";
    case RoomMember::Error::CouldNotConnect:
        return "CouldNotConnect";
    case RoomMember::Error::RoomIsFull:
        return "RoomIsFull";
    case RoomMember::Error::HostBanned:
        return "HostBanned";
    default:
        return "Unknown";
    }
}

} // namespace Network
