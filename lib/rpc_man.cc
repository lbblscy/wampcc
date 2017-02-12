#include "wampcc/rpc_man.h"

#include "wampcc/event_loop.h"
#include "wampcc/kernel.h"
#include "wampcc/log_macros.h"
#include "wampcc/wamp_session.h"

#include <memory>

namespace wampcc {



/* Constructor */
rpc_man::rpc_man(kernel* k, rpc_added_cb cb)
  : __logger(k->get_logger()),
    m_rpc_added_cb(cb),
    m_next_regid(1)
{
}




rpc_details rpc_man::get_rpc_details( const std::string& rpcname,
                                      const std::string& realm )
{
  std::lock_guard< std::mutex > guard ( m_rpc_map_lock );

  auto realm_iter = m_realm_to_registry.find( realm );

  if (realm_iter == m_realm_to_registry.end())
    return rpc_details(); // realm not found

  auto rpc_iter = realm_iter->second.find(rpcname);
  if (rpc_iter == realm_iter->second.end())
    return rpc_details(); // procedure not found

  return rpc_iter->second;
}


int rpc_man::register_internal_rpc_2(const std::string& realm,
                                     const std::string& uri,
                                     const jalson::json_object& /*options*/,
                                     rpc_cb user_cb,
                                     void * user_data)
{
  rpc_details r;
  r.uri = uri;
  r.user_cb = user_cb;
  r.user_data = user_data;
  r.type = rpc_details::eInternal;

  register_rpc(session_handle(), realm, r);

  if (m_rpc_added_cb) m_rpc_added_cb( r );
  return r.registration_id;
}


uint64_t rpc_man::handle_inbound_register(session_handle sh,
                                          std::string realm,
                                          std::string ___uri)
{
  /* EV thread */

  rpc_details r;
  r.registration_id = 0;
  r.uri = std::move(___uri);
  r.session = sh;
  r.type = rpc_details::eRemote;

  register_rpc(sh, realm, r);

  if (m_rpc_added_cb) m_rpc_added_cb( r );
  return r.registration_id;
}


void rpc_man::register_rpc(session_handle session, std::string realm, rpc_details& r)
{
  std::lock_guard< std::mutex > guard ( m_rpc_map_lock );
  auto realm_iter = m_realm_to_registry.find( realm );

  // add realm if not already present
  if (realm_iter == m_realm_to_registry.end())
  {
    auto p = m_realm_to_registry.insert(std::make_pair(realm, rpc_registry()));
    realm_iter = std::move(p.first);
  }

  auto result = realm_iter->second.insert(std::make_pair(r.uri,r));
  if (!result.second)
  {
    LOG_WARN("Ignore duplicate procedure register for " << realm << ":" << r.uri);
    throw wamp_error(WAMP_ERROR_PROCEDURE_ALREADY_EXISTS);
  }

  r.registration_id = m_next_regid++;
  result.first->second = r;

  auto & rpcs_for_session = m_rpcs_for_session[session];
  rpcs_for_session.push_back( result.first );

  LOG_INFO( "procedure added, " << r.registration_id << ", "
            << realm << "::"
            << r.uri );
}


void rpc_man::session_closed(std::shared_ptr<wamp_session>& session)
{
  /* EV thread */

  std::lock_guard< std::mutex > guard ( m_rpc_map_lock );

  auto realm_iter = m_realm_to_registry.find( session->realm() );
  if (realm_iter != m_realm_to_registry.end())
  {
    auto session_iter = m_rpcs_for_session.find(session);
    if (session_iter != m_rpcs_for_session.end())
      for (auto & rpc_iter : session_iter->second)
      {
        LOG_INFO( "procedure removed, " << rpc_iter->second.registration_id << ", "
                  << session->realm() << "::"
                  << rpc_iter->first );
        realm_iter->second.erase( rpc_iter );
      }
  }

  m_rpcs_for_session.erase(session);
}


} // namespace wampcc
