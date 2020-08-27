#ifndef _PRINTOUT_H
#define _PRINTOUT_H

#include <td/telegram/td_api.hpp>

std::string requestToString(const td::TlObject &req);
std::string responseToString(const td::TlObject &object);

#endif
