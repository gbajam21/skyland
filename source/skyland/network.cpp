#include "network.hpp"



namespace skyland {



namespace network {



void poll_messages(Platform& pfrm, App& app, Listener& listener)
{
    while (auto message = pfrm.network_peer().poll_message()) {
        if (message->length_ < sizeof(packet::Header)) {
            return;
        }
        packet::Header header;
        memcpy(&header, message->data_, sizeof header);

        switch (header.message_type_) {
        case packet::Header::null:
            pfrm.network_peer().poll_consume(sizeof(packet::Header));
            continue;

#define HANDLE_MESSAGE(MESSAGE_TYPE)                                           \
    case MESSAGE_TYPE::mt: {                                                   \
        NET_EVENT_SIZE_CHECK(MESSAGE_TYPE)                                     \
        if (message->length_ < sizeof(MESSAGE_TYPE)) {                         \
            return;                                                            \
        }                                                                      \
        MESSAGE_TYPE m;                                                        \
        memcpy(&m, message->data_, sizeof m);                                  \
        pfrm.network_peer().poll_consume(sizeof(MESSAGE_TYPE));                \
        listener.receive(pfrm, app, m);                                        \
        continue;                                                              \
    }

            HANDLE_MESSAGE(packet::RoomConstructed)
            HANDLE_MESSAGE(packet::RoomSalvaged)
            HANDLE_MESSAGE(packet::TerrainConstructed)
            HANDLE_MESSAGE(packet::WeaponSetTarget)
            HANDLE_MESSAGE(packet::CharacterSetTarget)
            HANDLE_MESSAGE(packet::RoomDestroyed)
            HANDLE_MESSAGE(packet::CharacterBoarded)
            HANDLE_MESSAGE(packet::CharacterDisembark)
            HANDLE_MESSAGE(packet::CharacterDied)
            HANDLE_MESSAGE(packet::ReplicantCreated)
            HANDLE_MESSAGE(packet::OpponentBulkheadChanged)
        }

        error(pfrm, "garbled message!?");
        pfrm.network_peer().disconnect();
        return;
    }
}



} // namespace network



} // namespace skyland
