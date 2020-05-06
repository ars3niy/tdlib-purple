#ifndef _PRINTOUT_H
#define _PRINTOUT_H

#include <td/telegram/td_api.hpp>

std::string requestToString(const td::td_api::Function &req);
std::string responseToString(const td::td_api::Object &object);

#endif
