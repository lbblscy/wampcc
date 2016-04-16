#ifndef XXX_PUBSUB_MAN_H
#define XXX_PUBSUB_MAN_H

#include <map>

namespace XXX {

class Logger;
class client_service;
class ev_inbound_publish;
class event_loop;
class managed_topic;
class SessionMan;
class session_state_event;
class ev_inbound_message;

class pubsub_man
{
public:
  pubsub_man(Logger *, event_loop&, SessionMan&);
  ~pubsub_man();

  void handle_event(ev_inbound_publish*);
  void handle_subscribe(ev_inbound_message* ev);
  void handle_event( session_state_event* );;

private:
  pubsub_man(const pubsub_man&); // no copy
  pubsub_man& operator=(const pubsub_man&); // no assignment

  managed_topic* find_topic(const std::string& topic,
                            const std::string& realm,
                            bool allow_create);

  Logger *__logptr; /* name chosen for log macros */
  event_loop& m_evl;
  SessionMan& m_sesman;

  typedef  std::map< std::string, managed_topic* > topic_registry;
  typedef  std::map< std::string, topic_registry >   realm_to_topicreg;
  realm_to_topicreg m_topics; // TODO:change to unqiue_ptr
  size_t m_next_subscription_id;
};

} // namespace XXX

#endif
