//
// Based on tdlib C++ example Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2020
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "config.h"
#include "td-client.h"
#include <purple.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Simple single-threaded example of TDLib usage.
// Real world programs should use separate thread for the user input.
// Example includes user authentication, receiving updates, getting chat list and sending text messages.

// overloaded
namespace detail {
template <class... Fs>
struct overload;

template <class F>
struct overload<F> : public F {
  explicit overload(F f) : F(f) {
  }
};
template <class F, class... Fs>
struct overload<F, Fs...>
    : public overload<F>
    , overload<Fs...> {
  overload(F f, Fs... fs) : overload<F>(f), overload<Fs...>(fs...) {
  }
  using overload<F>::operator();
  using overload<Fs...>::operator();
};
}  // namespace detail

template <class... F>
auto overloaded(F... f) {
  return detail::overload<F...>(f...);
}

namespace td_api = td::td_api;

class TdExample {
 public:
  TdExample() {
    td::Client::execute({0, td_api::make_object<td_api::setLogVerbosityLevel>(1)});
    client_ = std::make_unique<td::Client>();
  }

  void loop() {
    while (true) {
      if (need_restart_) {
        restart();
      } else if (!are_authorized_) {
        process_response(client_->receive(10));
      } else {
        std::cout << "Enter action [q] quit [u] check for updates and request results [c] show chats [m <id> <text>] "
                     "send message [me] show self [l] logout: "
                  << std::endl;
        std::string line;
        std::getline(std::cin, line);
        std::istringstream ss(line);
        std::string action;
        if (!(ss >> action)) {
          continue;
        }
        if (action == "q") {
          return;
        }
        if (action == "u") {
          std::cout << "Checking for updates..." << std::endl;
          while (true) {
            auto response = client_->receive(0);
            if (response.object) {
              process_response(std::move(response));
            } else {
              break;
            }
          }
        } else if (action == "close") {
          std::cout << "Closing..." << std::endl;
          send_query(td_api::make_object<td_api::close>(), {});
        } else if (action == "me") {
          send_query(td_api::make_object<td_api::getMe>(),
                     [this](Object object) { std::cout << to_string(object) << std::endl; });
        } else if (action == "l") {
          std::cout << "Logging out..." << std::endl;
          send_query(td_api::make_object<td_api::logOut>(), {});
        } else if (action == "m") {
          std::int64_t chat_id;
          ss >> chat_id;
          ss.get();
          std::string text;
          std::getline(ss, text);

          std::cout << "Sending message to chat " << chat_id << "..." << std::endl;
          auto send_message = td_api::make_object<td_api::sendMessage>();
          send_message->chat_id_ = chat_id;
          auto message_content = td_api::make_object<td_api::inputMessageText>();
          message_content->text_ = td_api::make_object<td_api::formattedText>();
          message_content->text_->text_ = std::move(text);
          send_message->input_message_content_ = std::move(message_content);

          send_query(std::move(send_message), {});
        } else if (action == "c") {
          std::cout << "Loading chat list..." << std::endl;
          send_query(td_api::make_object<td_api::getChats>(nullptr, std::numeric_limits<std::int64_t>::max(), 0, 20),
                     [this](Object object) {
                       if (object->get_id() == td_api::error::ID) {
                         return;
                       }
                       auto chats = td::move_tl_object_as<td_api::chats>(object);
                       for (auto chat_id : chats->chat_ids_) {
                         std::cout << "[id:" << chat_id << "] [title:" << chat_title_[chat_id] << "]" << std::endl;
                       }
                     });
        }
      }
    }
  }

 private:
  using Object = td_api::object_ptr<td_api::Object>;
  std::unique_ptr<td::Client> client_;

  td_api::object_ptr<td_api::AuthorizationState> authorization_state_;
  bool are_authorized_{false};
  bool need_restart_{false};
  std::uint64_t current_query_id_{0};
  std::uint64_t authentication_query_id_{0};

  std::map<std::uint64_t, std::function<void(Object)>> handlers_;

  std::map<std::int32_t, td_api::object_ptr<td_api::user>> users_;

  std::map<std::int64_t, std::string> chat_title_;

  void restart() {
    client_.reset();
    *this = TdExample();
  }

  void send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
      handlers_.emplace(query_id, std::move(handler));
    }
    client_->send({query_id, std::move(f)});
  }

  void process_response(td::Client::Response response) {
    if (!response.object) {
      return;
    }
    //std::cout << response.id << " " << to_string(response.object) << std::endl;
    if (response.id == 0) {
      return process_update(std::move(response.object));
    }
    auto it = handlers_.find(response.id);
    if (it != handlers_.end()) {
      it->second(std::move(response.object));
    }
  }

  std::string get_user_name(std::int32_t user_id) {
    auto it = users_.find(user_id);
    if (it == users_.end()) {
      return "unknown user";
    }
    return it->second->first_name_ + " " + it->second->last_name_;
  }

  void process_update(td_api::object_ptr<td_api::Object> update) {
    td_api::downcast_call(
        *update, overloaded(
                     [this](td_api::updateAuthorizationState &update_authorization_state) {
                       authorization_state_ = std::move(update_authorization_state.authorization_state_);
                       on_authorization_state_update();
                     },
                     [this](td_api::updateNewChat &update_new_chat) {
                       chat_title_[update_new_chat.chat_->id_] = update_new_chat.chat_->title_;
                     },
                     [this](td_api::updateChatTitle &update_chat_title) {
                       chat_title_[update_chat_title.chat_id_] = update_chat_title.title_;
                     },
                     [this](td_api::updateUser &update_user) {
                       auto user_id = update_user.user_->id_;
                       users_[user_id] = std::move(update_user.user_);
                     },
                     [this](td_api::updateNewMessage &update_new_message) {
                       auto chat_id = update_new_message.message_->chat_id_;
                       auto sender_user_name = get_user_name(update_new_message.message_->sender_user_id_);
                       std::string text;
                       if (update_new_message.message_->content_->get_id() == td_api::messageText::ID) {
                         text = static_cast<td_api::messageText &>(*update_new_message.message_->content_).text_->text_;
                       }
                       std::cout << "Got message: [chat_id:" << chat_id << "] [from:" << sender_user_name << "] ["
                                 << text << "]" << std::endl;
                     },
                     [](auto &update) {}));
  }

  auto create_authentication_query_handler() {
    return [this, id = authentication_query_id_](Object object) {
      if (id == authentication_query_id_) {
        check_authentication_error(std::move(object));
      }
    };
  }

  void on_authorization_state_update() {
    authentication_query_id_++;
    td_api::downcast_call(
        *authorization_state_,
        overloaded(
            [this](td_api::authorizationStateReady &) {
              are_authorized_ = true;
              std::cout << "Got authorization" << std::endl;
            },
            [this](td_api::authorizationStateLoggingOut &) {
              are_authorized_ = false;
              std::cout << "Logging out" << std::endl;
            },
            [this](td_api::authorizationStateClosing &) { std::cout << "Closing" << std::endl; },
            [this](td_api::authorizationStateClosed &) {
              are_authorized_ = false;
              need_restart_ = true;
              std::cout << "Terminated" << std::endl;
            },
            [this](td_api::authorizationStateWaitCode &) {
              std::cout << "Enter authentication code: " << std::flush;
              std::string code;
              std::cin >> code;
              send_query(td_api::make_object<td_api::checkAuthenticationCode>(code),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitRegistration &) {
              std::string first_name;
              std::string last_name;
              std::cout << "Enter your first name: " << std::flush;
              std::cin >> first_name;
              std::cout << "Enter your last name: " << std::flush;
              std::cin >> last_name;
              send_query(td_api::make_object<td_api::registerUser>(first_name, last_name),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitPassword &) {
              std::cout << "Enter authentication password: " << std::flush;
              std::string password;
              std::cin >> password;
              send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitOtherDeviceConfirmation &state) {
              std::cout << "Confirm this login link on another device: " << state.link_ << std::endl;
            },
            [this](td_api::authorizationStateWaitPhoneNumber &) {
              std::cout << "Enter phone number: " << std::flush;
              std::string phone_number;
              std::cin >> phone_number;
              send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone_number, nullptr),
                         create_authentication_query_handler());
            },
            [this](td_api::authorizationStateWaitEncryptionKey &) {
              std::cout << "Enter encryption key or DESTROY: " << std::flush;
              std::string key;
              std::getline(std::cin, key);
              if (key == "DESTROY") {
                send_query(td_api::make_object<td_api::destroy>(), create_authentication_query_handler());
              } else {
                send_query(td_api::make_object<td_api::checkDatabaseEncryptionKey>(std::move(key)),
                           create_authentication_query_handler());
              }
            },
            [this](td_api::authorizationStateWaitTdlibParameters &) {
              auto parameters = td_api::make_object<td_api::tdlibParameters>();
              parameters->database_directory_ = "tdlib";
              parameters->use_message_database_ = true;
              parameters->use_secret_chats_ = true;
              parameters->api_id_ = 94575;
              parameters->api_hash_ = "a3406de8d171bb422bb6ddf3bbd800e2";
              parameters->system_language_code_ = "en";
              parameters->device_model_ = "Desktop";
              parameters->system_version_ = "Unknown";
              parameters->application_version_ = "1.0";
              parameters->enable_storage_optimizer_ = true;
              send_query(td_api::make_object<td_api::setTdlibParameters>(std::move(parameters)),
                         create_authentication_query_handler());
            }));
  }

  void check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
      auto error = td::move_tl_object_as<td_api::error>(object);
      std::cout << "Error: " << to_string(error) << std::flush;
      on_authorization_state_update();
    }
  }

  std::uint64_t next_query_id() {
    return ++current_query_id_;
  }
};

static const char *tgprpl_list_icon (PurpleAccount *acct, PurpleBuddy *buddy)
{
    return "telegram";
}

static void tgprpl_tooltip_text (PurpleBuddy *buddy, PurpleNotifyUserInfo *info, gboolean full)
{
    purple_notify_user_info_add_pair (info, "test: ", "TEST");
}

static GList *tgprpl_status_types (PurpleAccount *acct)
{
    GList *types = NULL;
    PurpleStatusType *type;

    type = purple_status_type_new_full (PURPLE_STATUS_AVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_MOBILE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_OFFLINE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    /*
    The states below are only registered internally so that we get notified about
    state changes to away and unavailable. This is useful for deciding when to send
    No other peer should ever have those states.
    */
    type = purple_status_type_new_full (PURPLE_STATUS_AWAY, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    type = purple_status_type_new_full (PURPLE_STATUS_UNAVAILABLE, NULL, NULL, FALSE, TRUE, FALSE);
    types = g_list_prepend (types, type);

    return g_list_reverse (types);
}

static GList* tgprpl_blist_node_menu (PurpleBlistNode *node)
{
    return NULL;
}

static const char *_(const char *s) { return s; }
static char *_(char *s) { return s; }

static GList *tgprpl_chat_join_info (PurpleConnection *gc)
{
    struct proto_chat_entry *pce;
    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Subject:");
    pce->identifier = "subject";
    pce->required = FALSE;
    GList *info = g_list_append (NULL, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Invite link:");
    pce->identifier = "link";
    pce->required = FALSE;
    info = g_list_append (info, pce);

    pce = g_new0 (struct proto_chat_entry, 1);
    pce->label = _("Chat ID:");
    pce->identifier = "id";
    pce->required = FALSE;

    return g_list_append (info, pce);
}

static GHashTable *tgprpl_chat_info_defaults (PurpleConnection *gc, const char *chat_name)
{
    return g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
}

static void tgprpl_login (PurpleAccount * acct)
{
    PurpleConnection *gc       = purple_account_get_connection (acct);
    PurpleTdClient   *tdClient = new PurpleTdClient;

    purple_connection_set_protocol_data (gc, tdClient);
    purple_connection_set_state (gc, PURPLE_CONNECTING);
    tdClient->startLogin();
    purple_connection_set_state (gc, PURPLE_CONNECTED);

    purple_blist_add_account (acct);
}

static void tgprpl_close (PurpleConnection *gc)
{
    delete static_cast<PurpleTdClient *>(purple_connection_get_protocol_data(gc));
}

static int tgprpl_send_im (PurpleConnection *gc, const char *who, const char *message, PurpleMessageFlags flags)
{
    return -1;
}

static unsigned int tgprpl_send_typing (PurpleConnection *gc, const char *who, PurpleTypingState typing)
{
    return 0;
}

static void tgprpl_info_show (PurpleConnection *gc, const char *who)
{
    //tgl_do_get_channel_info (gc_get_tls (gc), P->id, FALSE, tgp_info_load_channel_done, P);
    //tgl_do_get_user_info (gc_get_tls (gc), P->id, 0, tgp_info_load_user_done, P);
}

static void tgprpl_set_status (PurpleAccount *acct, PurpleStatus *status)
{
}

static void tgprpl_add_buddy (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
}

static void request_delete_contact_ok (void *data, PurpleRequestFields* fields)
{
}

static void request_delete_contact_cancel (void *data, PurpleRequestFields* fields)
{
}

static void tgprpl_request_delete_contact (PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
    const char *title1 = NULL, *title2 = NULL, *msg = NULL;

    g_return_if_fail(buddy);

    title1 = _("Test");
    title2 = title1;
    msg = _("Do you want to?");

    if (msg) {
        purple_request_ok_cancel(gc, title1, title2, msg,
            0, purple_connection_get_account(gc), "test", NULL,
            NULL, // user_data
            request_delete_contact_ok, request_delete_contact_cancel);
    }
}

static void tgprpl_chat_join (PurpleConnection *gc, GHashTable *data)
{
    purple_serv_got_join_chat_failed (gc, data);
}

static char *tgprpl_get_chat_name (GHashTable * data)
{
    return g_strdup("test");
}

static void tgprpl_chat_invite (PurpleConnection *gc, int id, const char *message, const char *who)
{
}

static int tgprpl_send_chat (PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags)
{
    return -1;
}

static PurpleRoomlist *tgprpl_roomlist_get_list (PurpleConnection *gc)
{
    static PurpleRoomlist *roomlist = NULL; // put it on like protocol_data

    if (roomlist)
        purple_roomlist_unref (roomlist);
    roomlist = purple_roomlist_new (purple_connection_get_account (gc));

    purple_roomlist_set_in_progress (roomlist, TRUE);
    // blah blah blah
    purple_roomlist_set_in_progress (roomlist, FALSE);

    return roomlist;
}

static void tgprpl_roomlist_cancel (PurpleRoomlist *list)
{
}

static gboolean tgprpl_can_receive_file (PurpleConnection *gc, const char *who)
{
    return TRUE;
}

static void tgprpl_xfer_send_init (PurpleXfer *X)
{
}

static void tgprpl_xfer_canceled (PurpleXfer *X)
{
}

static PurpleXfer *tgprpl_new_xfer (PurpleConnection *gc, const char *who)
{
    PurpleXfer *X = purple_xfer_new (purple_connection_get_account (gc), PURPLE_XFER_SEND, who);
    if (X) {
        purple_xfer_set_init_fnc (X, tgprpl_xfer_send_init);
        purple_xfer_set_cancel_send_fnc (X, tgprpl_xfer_canceled);
    }

    return X;
}

static void tgprpl_send_file (PurpleConnection * gc, const char *who, const char *file)
{
    PurpleXfer *X = tgprpl_new_xfer (gc, who);
    if (file) {
        purple_xfer_request_accepted (X, file);
    } else {
        purple_xfer_request (X);
    }
}

static GHashTable *tgprpl_get_account_text_table (PurpleAccount *pa)
{
    GHashTable *HT;
    HT = g_hash_table_new (g_str_hash, g_str_equal);
    static char label[] = "login_label";
    static char phoneno[] = "phone no. (+ country prefix)";
    g_hash_table_insert(HT, label, _(phoneno));
    return HT;
}

static char png[] = "png";

static PurplePluginProtocolInfo prpl_info = {
    (PurpleProtocolOptions)(OPT_PROTO_NO_PASSWORD | OPT_PROTO_IM_IMAGE),
    NULL,                    // user_¡splits, initialized in tgprpl_init()
    NULL,                    // protocol_options, initialized in tgprpl_init()
    {
        png,
        1,                     // min_width
        1,                     // min_height
        512,                   // max_width
        512,                   // max_height
        64000,                 // max_filesize
        PURPLE_ICON_SCALE_SEND,
    },
    tgprpl_list_icon,
    NULL,
    NULL,
    tgprpl_tooltip_text,
    tgprpl_status_types,
    tgprpl_blist_node_menu,
    tgprpl_chat_join_info,
    tgprpl_chat_info_defaults,
    tgprpl_login,
    tgprpl_close,
    tgprpl_send_im,
    NULL,                    // set_info
    tgprpl_send_typing,
    tgprpl_info_show,
    tgprpl_set_status,
    NULL,                    // set_idle
    NULL,                    // change_passwd
    tgprpl_add_buddy,
    NULL,                    // add_buddies
    tgprpl_request_delete_contact,
    NULL,                    // remove_buddies
    NULL,                    // add_permit
    NULL,                    // add_deny
    NULL,                    // rem_permit
    NULL,                    // rem_deny
    NULL,                    // set_permit_deny
    tgprpl_chat_join,
    NULL,                    // reject_chat
    tgprpl_get_chat_name,
    tgprpl_chat_invite,
    NULL,                    // chat_leave
    NULL,                    // chat_whisper
    tgprpl_send_chat,
    NULL,                    // keepalive
    NULL,                    // register_user
    NULL,                    // get_cb_info
    NULL,                    // get_cb_away
    NULL,                    // alias_buddy
    NULL,                    // group_buddy
    NULL,                    // rename_group
    NULL,                    // buddy_free
    NULL,                    // convo_closed
    NULL,                    // normalize
    NULL,                    // tgprpl_set_buddy_icon
    NULL,                    // remove_group
    NULL,
    NULL,                    // set_chat_topic
    NULL,                    // find_blist_chat
    tgprpl_roomlist_get_list,
    tgprpl_roomlist_cancel,
    NULL,                    // roomlist_expand_category
    tgprpl_can_receive_file,
    tgprpl_send_file,
    tgprpl_new_xfer,
    NULL,                    // offline_message
    NULL,                    // whiteboard_prpl_ops
    NULL,                    // send_raw
    NULL,                    // roomlist_room_serialize
    NULL,                    // unregister_user
    NULL,                    // send_attention
    NULL,                    // get_attention_types
    sizeof(PurplePluginProtocolInfo),
    tgprpl_get_account_text_table, // get_account_text_table
    NULL,                    // initiate_media
    NULL,                    // get_media_caps
    NULL,                    // get_moods
    NULL,                    // set_public_alias
    NULL,                    // get_public_alias
    NULL,                    // add_buddy_with_invite
    NULL                     // add_buddies_with_invite
};

static gboolean tgprpl_load (PurplePlugin *plugin)
{
    //purple_cmd_register("kick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT |
    //                    PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS,
    //                    PLUGIN_ID, tgprpl_cmd_kick,
    //                    _("kick <user>:  Kick a user from the room."), NULL);

    return TRUE;
}

static void tgprpl_init (PurplePlugin *plugin)
{
}

static GList *tgprpl_actions (PurplePlugin *plugin, gpointer context)
{
    return (GList *)NULL;
}

static PurplePluginInfo plugin_info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,
    NULL,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,
    config::pluginId,
    config::pluginName,
    config::versionString,
    config::pluginSummary,
    _(config::pluginDesc),
    config::pluginAuthor,
    config::projectUrl,
    tgprpl_load,    // on load
    NULL,           // on unload
    NULL,           // on destroy
    NULL,           // ui specific struct
    &prpl_info,
    NULL,           // prefs info
    tgprpl_actions,
    NULL,           // reserved
    NULL,           // reserved
    NULL,           // reserved
    NULL            // reserved
};

extern "C" {
    PURPLE_INIT_PLUGIN (telegram_tdlib, tgprpl_init, plugin_info)
}
