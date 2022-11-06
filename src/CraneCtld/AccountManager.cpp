#include "AccountManager.h"

namespace Ctld {

AccountManager::AccountManager() { InitDataMap(); }

AccountManager::Result AccountManager::AddUser(Ctld::User& new_user) {
  // Avoid duplicate insertion
  auto find_user_iterator = m_user_map_.find(new_user.name);
  if (find_user_iterator != m_user_map_.end() &&
      !find_user_iterator->second->deleted) {
    return Result{false,
                  fmt::format("The user {} already exists in the database",
                              new_user.name)};
  }

  if (!new_user.account.empty()) {
    // Check whether the user's account exists
    auto find_account_iterator = m_account_map_.find(new_user.account);
    if (find_account_iterator != m_account_map_.end() &&
        !find_account_iterator->second->deleted) {
      Ctld::Account* parent_ptr = find_account_iterator->second.get();
      std::list<std::string>& parent_allowed_partition =
          parent_ptr->allowed_partition;
      if (!new_user.allowed_partition_qos_map.empty()) {
        // Check if user allowed partition is a subset of parent allowed
        // partition
        for (auto&& [partition, qos] : new_user.allowed_partition_qos_map) {
          if (std::find(parent_allowed_partition.begin(),
                        parent_allowed_partition.end(),
                        partition) != parent_allowed_partition.end()) {
            qos.first = parent_ptr->default_qos;
            qos.second = std::list<std::string>{parent_ptr->allowed_qos_list};
          } else {
            return Result{
                false, fmt::format("Partition {} is not allowed in account {}",
                                   partition, parent_ptr->name)};
          }
        }
      } else {
        // Inherit
        for (const auto& partition : parent_allowed_partition) {
          new_user.allowed_partition_qos_map[partition] =
              std::pair<std::string, std::list<std::string>>{
                  parent_ptr->default_qos,
                  std::list<std::string>{parent_ptr->allowed_qos_list}};
        }
      }

      parent_ptr->users.emplace_back(new_user.name);
    } else {
      return Result{false,
                    fmt::format("The account {} doesn't exist in the database",
                                new_user.account)};
    }

    // Update the user's account's users_list
    if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                      "$addToSet", new_user.account, "users",
                                      new_user.name)) {
      return AccountManager::Result{
          false, fmt::format("Failed to update the user list of the account {}",
                             new_user.account)};
    }
  } else {
    return Result{false, fmt::format("Please specify the user's account")};
  }

  if (find_user_iterator != m_user_map_.end() &&
      find_user_iterator->second
          ->deleted) {  // There is a same user but was deleted
    // Overwrite the deleted user with the same name
    find_user_iterator->second.reset();
    m_user_map_[new_user.name] = std::make_unique<Ctld::User>(new_user);

    if (g_db_client->UpdateUser(new_user))
      return Result{true};
    else
      return Result{false, "Can't update the deleted user to database"};
  } else {
    // Insert the new user
    m_user_map_[new_user.name] = std::make_unique<Ctld::User>(new_user);

    if (g_db_client->InsertUser(new_user)) {
      return Result{true};
    } else {
      return Result{false, "Can't insert the new user to database"};
    }
  }
}

AccountManager::Result AccountManager::AddAccount(Ctld::Account& new_account) {
  // Avoid duplicate insertion
  auto find_account_iterator = m_account_map_.find(new_account.name);
  if (find_account_iterator != m_account_map_.end() &&
      !find_account_iterator->second->deleted) {
    return Result{false,
                  fmt::format("The account {} already exists in the database",
                              new_account.name)};
  }

  if (!new_account.parent_account.empty()) {
    // Check whether the account's parent account exists
    auto find_parent_iterator = m_account_map_.find(new_account.parent_account);
    if (find_parent_iterator != m_account_map_.end() &&
        !find_parent_iterator->second->deleted) {
      find_parent_iterator->second->child_account.emplace_back(
          new_account.name);
      if (new_account.allowed_partition.empty()) {
        // Inherit
        new_account.allowed_partition = std::list<std::string>{
            find_parent_iterator->second->allowed_partition};
      }
    } else {
      return Result{
          false,
          fmt::format("The parent account {} doesn't exist in the database",
                      new_account.parent_account)};
    }

    // update the parent account's child_account_list
    if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                      "$addToSet", new_account.parent_account,
                                      "child_account_list", new_account.name)) {
      return Result{false, fmt::format("Failed to update the child account "
                                       "list of the parent account {}",
                                       new_account.parent_account)};
    }
  }

  if (find_account_iterator != m_account_map_.end() &&
      find_account_iterator->second
          ->deleted) {  // There is a same account but was deleted
    // Overwrite the deleted account with the same name
    find_account_iterator->second.reset();
    m_account_map_[new_account.name] =
        std::make_unique<Ctld::Account>(new_account);
    if (g_db_client->UpdateAccount(new_account)) {
      return Result{true};
    } else {
      return Result{false, "Can't update the deleted account to database"};
    }
  } else {
    // Insert the new account
    m_account_map_[new_account.name] =
        std::make_unique<Ctld::Account>(new_account);

    if (g_db_client->InsertAccount(new_account)) {
      return Result{true};
    } else {
      return Result{false, "Can't insert the new account to database"};
    }
  }
}

AccountManager::Result AccountManager::AddQos(Ctld::Qos& new_qos) {
  //  auto builder = bsoncxx::builder::stream::document{};
  //  bsoncxx::document::value doc_value =
  //      builder << "name" << new_qos.name << "description" <<
  //      new_qos.description
  //              << "priority" << new_qos.priority << "max_jobs_per_user"
  //              << new_qos.max_jobs_per_user
  //              << bsoncxx::builder::stream::
  //                     finalize;  // Use bsoncxx::builder::stream::finalize to
  //                                // obtain a bsoncxx::document::value
  //                                instance.
  //  stdx::optional<result::insert_one> ret;
  //  if (m_dbInstance && m_client) {
  //    ret = m_user_collection->insert_one(doc_value.view());
  //  }
  //
  //  return Result{ret != stdx::nullopt};
}

AccountManager::Result AccountManager::DeleteUser(const std::string& name) {
  auto find_user_iterator = m_user_map_.find(name);
  if (find_user_iterator == m_user_map_.end()) {
    return Result{false,
                  fmt::format("User {} doesn't exist in the database", name)};
  } else if (find_user_iterator->second->deleted) {
    return Result{false, fmt::format("User {} has been deleted", name)};
  }

  Ctld::User* user_ptr = find_user_iterator->second.get();

  if (!user_ptr->account.empty()) {
    // delete form the parent account's users list
    auto find_account_iterator = m_account_map_.find(user_ptr->account);
    find_account_iterator->second->users.remove(name);
    if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                      "$pull", user_ptr->account, "users",
                                      name)) {
      return Result{
          false,
          fmt::format("Can't delete {} form the account's users list", name)};
    }
  }
  // Delete the user
  //  m_user_map_.erase(find_user_iterator);
  find_user_iterator->second->deleted = true;
  if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::User, "$set",
                                    name, "deleted", true)) {
    return Result{false, fmt::format("Delete user {} failed", name)};
  }

  return Result{true};
}

AccountManager::Result AccountManager::DeleteAccount(const std::string& name) {
  auto find_account_iterator = m_account_map_.find(name);
  if (find_account_iterator == m_account_map_.end()) {
    return Result{
        false, fmt::format("Account {} doesn't exist in the database", name)};
  } else if (find_account_iterator->second->deleted) {
    return Result{false, fmt::format("Account {} has been deleted", name)};
  }
  Ctld::Account* account_ptr = find_account_iterator->second.get();
  if (!account_ptr->child_account.empty() || !account_ptr->users.empty()) {
    return Result{false, "This account has child account or users"};
  }

  if (!account_ptr->parent_account.empty()) {
    // delete form the parent account's child account list
    if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                      "$pull", account_ptr->parent_account,
                                      "child_account", name)) {
      return Result{
          false,
          fmt::format("Can't delete {} form the account's child accounts list",
                      name)};
    }
  }
  // Delete the account
  //  find_account_iterator->second.reset();
  //  m_account_map_.erase(find_account_iterator);
  find_account_iterator->second->deleted = true;
  if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account, "$set",
                                    name, "deleted", true)) {
    return Result{false, fmt::format("Delete account {} failed", name)};
  }

  return Result{true};
}

AccountManager::Result AccountManager::DeleteQos(const std::string& name) {
  return Result{true};
}
/**
 *
 * @param name
 * @return user pointer to m_user_map_
 */
Ctld::User* AccountManager::GetUserInfo(const std::string& name) {
  auto find_res = m_user_map_.find(name);
  if (find_res == m_user_map_.end()) {
    return nullptr;
  } else {
    return find_res->second.get();
  }
}

/*
 * Get the user info form mongodb and deletion flag marked false
 */
Ctld::User* AccountManager::GetExistedUserInfo(const std::string& name) {
  Ctld::User* user = GetUserInfo(name);
  if (user != nullptr && !user->deleted) {
    return user;
  } else {
    return nullptr;
  }
}

void AccountManager::GetAllUserInfo(std::list<Ctld::User>& user_list) {
  for (const auto& [name, user] : m_user_map_) {
    user_list.push_back(*user);
  }
}

Ctld::Account* AccountManager::GetAccountInfo(const std::string& name) {
  auto find_res = m_account_map_.find(name);
  if (find_res == m_account_map_.end()) {
    return nullptr;
  } else {
    return find_res->second.get();
  }
}

Ctld::Account* AccountManager::GetExistedAccountInfo(const std::string& name) {
  Ctld::Account* account = GetAccountInfo(name);
  if (account != nullptr && !account->deleted) {
    return account;
  } else {
    return nullptr;
  }
}

void AccountManager::GetAllAccountInfo(std::list<Ctld::Account>& account_list) {
  for (const auto& [name, account] : m_account_map_) {
    account_list.push_back(*account);
  }
}

bool AccountManager::GetQosInfo(const std::string& name, Ctld::Qos* qos) {
  auto find_res = m_qos_map_.find(name);
  if (find_res == m_qos_map_.end()) {
    return false;
  } else {
    *qos = *find_res->second;
    return true;
  }
}

std::unordered_map<std::string /*Qos name*/, std::unique_ptr<Ctld::Qos>>&
AccountManager::GetAllQosInfo() {
  return m_qos_map_;
}

AccountManager::Result AccountManager::ModifyUser(
    const crane::grpc::ModifyEntityRequest_OperatorType& operatorType,
    const std::string& name, const std::string& partition,
    const std::string& itemLeft, const std::string& itemRight) {
  Ctld::User* user = GetUserInfo(name);
  if (user == nullptr) {
    return Result{false, fmt::format("Unknown user {}", name)};
  }
  Ctld::Account* account = GetAccountInfo(user->account);
  switch (operatorType) {
    case crane::grpc::ModifyEntityRequest_OperatorType_Add:
      if (itemLeft == "allowed_partition") {
        // TODO: check if new partition existed

        // check if account has access to new partition
        if (std::find(account->allowed_partition.begin(),
                      account->allowed_partition.end(),
                      itemRight) == account->allowed_partition.end()) {
          return Result{
              false,
              fmt::format(
                  "User {}'s account {} not allow to use the partition {}",
                  name, user->account, itemRight)};
        }
        // check if add item already the user's allowed partition
        for (const auto& par : user->allowed_partition_qos_map) {
          if (itemRight == par.first) {
            return Result{false, fmt::format("The partition {} is already in "
                                             "user {}'s allowed partition",
                                             itemRight, name)};
          }
        }
        // Update the map
        user->allowed_partition_qos_map[itemRight] =
            std::pair<std::string, std::list<std::string>>{
                account->default_qos,
                std::list<std::string>{account->allowed_qos_list}};
      } else if (itemLeft == "allowed_qos_list") {
        // TODO: check if qos existed

        // check if account has access to new qos
        if (std::find(account->allowed_qos_list.begin(),
                      account->allowed_qos_list.end(),
                      itemRight) == account->allowed_qos_list.end()) {
          return Result{
              false,
              fmt::format("Sorry, your account not allow to use the qos {}",
                          itemRight)};
        }
        // check if add item already the user's allowed qos
        if (partition.empty()) {
          // add to all partition
          bool isChange = false;
          for (auto& [par, qos] : user->allowed_partition_qos_map) {
            std::list<std::string>& list = qos.second;
            if (std::find(list.begin(), list.end(), itemRight) == list.end()) {
              list.emplace_back(itemRight);
              isChange = true;
            }
          }
          if (!isChange) {
            return Result{false, fmt::format("Qos {} is already in user {}'s "
                                             "allowed qos of all partition",
                                             itemRight, name)};
          }
        } else {
          // add to exacted partition
          auto iter = user->allowed_partition_qos_map.find(partition);
          if (iter == user->allowed_partition_qos_map.end()) {
            return Result{
                false, fmt::format(
                           "Partition {} is not in user {}'s allowed partition",
                           partition, name)};
          }
          std::list<std::string>& list = iter->second.second;
          if (std::find(list.begin(), list.end(), itemRight) != list.end()) {
            return Result{false, fmt::format("Qos {} is already in user {}'s "
                                             "allowed qos of partition {}",
                                             itemRight, name, partition)};
          }
          list.push_back(itemRight);
        }
      } else {
        return Result{false, fmt::format("Field {} can't be added", itemLeft)};
      }
      break;
    case crane::grpc::ModifyEntityRequest_OperatorType_Overwrite:
      if (itemLeft == "admin_level") {
        Ctld::User::AdminLevel new_level;
        if (itemRight == "none") {
          new_level = Ctld::User::None;
        } else if (itemRight == "operator") {
          new_level = Ctld::User::Operator;
        } else if (itemRight == "admin") {
          new_level = Ctld::User::Admin;
        }
        if (new_level == user->admin_level) {
          return Result{false, fmt::format("User {} is already a {} role", name,
                                           itemRight)};
        }
        user->admin_level = new_level;
      }
      //      else if (itemLeft == "account") {
      //        Account* new_account = GetAccountInfo(itemRight);
      //        if(new_account == nullptr){
      //          return Result{ false, fmt::format("Account {} not existed",
      //          itemRight)};
      //        }
      //        account->users.remove(name);
      //        new_account->users.emplace_back(name);
      //      }
      else if (itemLeft == "default_qos") {
        if (partition.empty()) {
          bool isChange = false;
          for (auto& [par, qos] : user->allowed_partition_qos_map) {
            if (std::find(qos.second.begin(), qos.second.end(), itemRight) !=
                    qos.second.end() &&
                itemRight != qos.first) {
              isChange = true;
              qos.first = itemRight;
            }
          }
          if (!isChange) {
            return Result{false, fmt::format("Qos {} not in allowed qos list "
                                             "or is already the default qos",
                                             itemRight)};
          }
        } else {
          auto iter = user->allowed_partition_qos_map.find(partition);
          if (std::find(iter->second.second.begin(), iter->second.second.end(),
                        itemRight) == iter->second.second.end()) {
            return Result{false, fmt::format("Qos {} not in allowed qos list",
                                             itemRight)};
          }
          if (iter->second.first == itemRight) {
            return Result{
                false,
                fmt::format("Qos {} is already the default qos", itemRight)};
          }
          iter->second.first = itemRight;
        }
      } else {
        return Result{false, fmt::format("Field {} can't be set", itemLeft)};
      }
      break;
    case crane::grpc::ModifyEntityRequest_OperatorType_Delete:
      if (itemLeft == "allowed_partition") {
        auto iter = user->allowed_partition_qos_map.find(itemRight);
        if (iter == user->allowed_partition_qos_map.end()) {
          return Result{
              false,
              fmt::format(
                  "Partition {} is not in user {}'s allowed partition list",
                  itemRight, name)};
        }
        user->allowed_partition_qos_map.erase(iter);
      } else if (itemLeft == "allowed_qos_list") {
        if (partition.empty()) {
          bool isChange = false;
          for (auto& [par, qos] : user->allowed_partition_qos_map) {
            if (std::find(qos.second.begin(), qos.second.end(), itemRight) !=
                    qos.second.end() &&
                qos.first != itemRight) {
              isChange = true;
              qos.second.remove(itemRight);
            }
          }
          if (!isChange) {
            return Result{
                false,
                fmt::format("Qos {} not in allowed qos list or is default qos",
                            itemRight)};
          }
        } else {
          auto iter = user->allowed_partition_qos_map.find(partition);
          if (iter == user->allowed_partition_qos_map.end()) {
            return Result{
                false, fmt::format("Partition {} not in allowed partition list",
                                   partition)};
          }
          if (std::find(iter->second.second.begin(), iter->second.second.end(),
                        itemRight) == iter->second.second.end()) {
            return Result{
                false,
                fmt::format("Qos {} not in allowed qos list of partition {}",
                            itemRight, partition)};
          }
          if (itemRight == iter->second.first) {
            return Result{
                false,
                fmt::format(
                    "Qos {} is default qos of partition {},can't be deleted",
                    itemRight, partition)};
          }
          iter->second.second.remove(itemRight);
        }
      } else {
        return Result{false,
                      fmt::format("Field {} can't be deleted", itemLeft)};
      }
      break;
    default:
      return Result{false, fmt::format("Unknown field {}", itemLeft)};
      break;
  }
  // Update to database
  if (!g_db_client->UpdateUser(*user)) {
    return Result{false, "Can't update the  database"};
  } else {
    return Result{true};
  }
}

AccountManager::Result AccountManager::ModifyAccount(
    const crane::grpc::ModifyEntityRequest_OperatorType& operatorType,
    const std::string& name, const std::string& itemLeft,
    const std::string& itemRight) {
  std::string opt;
  Ctld::Account* account = GetAccountInfo(name);
  if (account == nullptr) {
    return Result{false, fmt::format("Unknown account {}", name)};
  }
  switch (operatorType) {
    case crane::grpc::ModifyEntityRequest_OperatorType_Add:
      opt = "$addToSet";
      if (itemLeft == "allowed_partition") {
        // TODO: check if the partition existed

        // Check if parent account has access to the partition
        if (!account->parent_account.empty()) {
          Ctld::Account* parent = GetAccountInfo(account->parent_account);
          if (std::find(parent->allowed_partition.begin(),
                        parent->allowed_partition.end(),
                        itemRight) == parent->allowed_partition.end()) {
            return Result{
                false,
                fmt::format(
                    "Parent account {} does not have access to partition {}",
                    account->parent_account, itemRight)};
          }
        }

        if (std::find(account->allowed_partition.begin(),
                      account->allowed_partition.end(),
                      itemRight) != account->allowed_partition.end()) {
          return Result{
              false,
              fmt::format("Partition {} is already in allowed partition list")};
        }
        account->allowed_partition.emplace_back(itemRight);
        if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                          opt, name, itemLeft, itemRight)) {
          return Result{false, "Can't update the  database"};
        } else {
          return Result{true};
        }
      } else if (itemLeft == "allowed_qos_list") {
        // TODO: check if the qos existed

        // Check if parent account has access to the qos
        if (!account->parent_account.empty()) {
          Ctld::Account* parent = GetAccountInfo(account->parent_account);
          if (std::find(parent->allowed_qos_list.begin(),
                        parent->allowed_qos_list.end(),
                        itemRight) == parent->allowed_qos_list.end()) {
            return Result{
                false,
                fmt::format("Parent account {} does not have access to qos {}",
                            account->parent_account, itemRight)};
          }
        }

        if (std::find(account->allowed_qos_list.begin(),
                      account->allowed_qos_list.end(),
                      itemRight) != account->allowed_qos_list.end()) {
          return Result{false,
                        fmt::format("Qos {} is already in allowed qos list")};
        }
        account->allowed_qos_list.emplace_back(itemRight);
        if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                          opt, name, itemLeft, itemRight)) {
          return Result{false, "Can't update the  database"};
        } else {
          return Result{true};
        }
      } else {
        return Result{false, fmt::format("Field {} can't be added", itemLeft)};
      }
      break;
    case crane::grpc::ModifyEntityRequest_OperatorType_Overwrite:
      opt = "$set";
      if (itemLeft == "description") {
        if (itemRight == account->description) {
          return Result{false, "Description content not change"};
        }
        account->description = itemRight;
        if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                          opt, name, itemLeft, itemRight)) {
          return Result{false, "Can't update the  database"};
        } else {
          return Result{true};
        }
      }
      //      else if (itemLeft == "parent_account") {
      //
      //        if(!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
      //        opt,
      //                                          name, itemLeft, itemRight)){
      //          return Result{ false , "Can't update the  database"};
      //        }
      //        else {
      //          return Result{true};
      //        }
      //      }
      else if (itemLeft == "default_qos") {
        if (account->default_qos == itemRight) {
          return Result{false, fmt::format("Qos {} is already the default qos",
                                           itemRight)};
        }
        if (std::find(account->allowed_qos_list.begin(),
                      account->allowed_qos_list.end(),
                      itemRight) == account->allowed_qos_list.end()) {
          return Result{
              false, fmt::format("Qos {} not in allowed qos list", itemRight)};
        }
        account->default_qos = itemRight;
        if (!g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account,
                                          opt, name, itemLeft, itemRight)) {
          return Result{false, "Can't update the  database"};
        } else {
          return Result{true};
        }
      } else {
        return Result{false, fmt::format("Field {} can't be set", itemLeft)};
      }
      break;
    case crane::grpc::ModifyEntityRequest_OperatorType_Delete:
      if (itemLeft == "allowed_partition") {
        if (!DeleteAccountAllowedPartition(GetAccountInfo(name), itemRight)) {
          return Result{
              false, fmt::format("Partition {} not in allowed partition list",
                                 itemRight)};
        }
      } else if (itemLeft == "allowed_qos_list") {
        if (!DeleteAccountAllowedQos(GetAccountInfo(name), itemRight, false)) {
          return Result{false, fmt::format("Qos {} not in allowed qos list or "
                                           "is used as default qos by someone",
                                           itemRight)};
        }
      } else {
        return Result{false,
                      fmt::format("Field {} can't be deleted", itemLeft)};
      }
      break;
    default:
      break;
  }
  return Result{true};
}

std::list<std::string> AccountManager::GetUserAllowedPartition(
    const std::string& name) {
  Ctld::User* user = GetExistedUserInfo(name);
  std::list<std::string> allowed_partition_list;
  for (const auto& [partition, qos] : user->allowed_partition_qos_map) {
    allowed_partition_list.emplace_back(partition);
  }
  return allowed_partition_list;
}

// std::list<std::string> AccountManager::GetAccountAllowedPartition(
//     const std::string& name) {
//   Ctld::Account* account = GetExistedAccountInfo(name);
//   return account->allowed_partition;
// }

void AccountManager::InitDataMap() {
  std::list<Ctld::User> user_list;
  g_db_client->SelectAllUser(user_list);
  for (auto& user : user_list) {
    m_user_map_[user.name] = std::make_unique<Ctld::User>(user);
  }

  std::list<Ctld::Account> account_list;
  g_db_client->SelectAllAccount(account_list);
  for (auto& account : account_list) {
    m_account_map_[account.name] = std::make_unique<Ctld::Account>(account);
  }

  std::list<Ctld::Qos> qos_list;
  g_db_client->SelectAllQos(qos_list);
  for (auto& qos : qos_list) {
    m_qos_map_[qos.name] = std::make_unique<Ctld::Qos>(qos);
  }
}

bool AccountManager::IsDefaultQosOfAnyPartition(Ctld::User* user,
                                                const std::string& qos) {
  for (auto& [par, qos_pair] : user->allowed_partition_qos_map) {
    if (qos_pair.first == qos) {
      return true;
    }
  }
  return false;
}

bool AccountManager::DeleteUserAllowedQosOfAllPartition(Ctld::User* user,
                                                        const std::string& qos,
                                                        bool force) {
  bool isDefault = IsDefaultQosOfAnyPartition(user, qos);
  if (!isDefault || force) {
    for (auto& [par, qos_pair] : user->allowed_partition_qos_map) {
      qos_pair.second.remove(qos);
      if (qos_pair.first == qos) {
        if (qos_pair.second.empty()) {
          qos_pair.first = "";
        } else {
          qos_pair.first = qos_pair.second.front();
        }
      }
    }
    g_db_client->UpdateUser(*user);
  } else {
    return false;
  }
  return true;
}

bool AccountManager::DeleteUserAllowedPartition(Ctld::User* user,
                                                const std::string& partition) {
  auto iter = user->allowed_partition_qos_map.find(partition);
  if (iter == user->allowed_partition_qos_map.end()) {
    return false;
  }
  user->allowed_partition_qos_map.erase(iter);
  g_db_client->UpdateUser(*user);
  return true;
}

bool AccountManager::IsDefaultQosOfAnyNode(Ctld::Account* account,
                                           const std::string& qos) {
  bool res = false;
  if (account->default_qos == qos) {
    return true;
  }
  for (const auto& child : account->child_account) {
    res |= IsDefaultQosOfAnyNode(GetAccountInfo(child), qos);
  }
  for (const auto& user : account->users) {
    res |= IsDefaultQosOfAnyPartition(GetUserInfo(user), qos);
  }
  return res;
}

bool AccountManager::DeleteAccountAllowedQos(Ctld::Account* account,
                                             const std::string& qos,
                                             bool force) {
  auto iter = std::find(account->allowed_qos_list.begin(),
                        account->allowed_qos_list.end(), qos);
  if (iter == account->allowed_qos_list.end()) {
    return false;
  }
  if (!IsDefaultQosOfAnyNode(account, qos) || force) {
    DeleteAccountAllowedQos_(account, qos);
  } else {
    return false;
  }
  return true;
}

bool AccountManager::DeleteAccountAllowedQos_(Ctld::Account* account,
                                              const std::string& qos) {
  auto iter = std::find(account->allowed_qos_list.begin(),
                        account->allowed_qos_list.end(), qos);
  if (iter == account->allowed_qos_list.end()) {
    return false;
  }

  for (const auto& child : account->child_account) {
    DeleteAccountAllowedQos_(GetAccountInfo(child), qos);
  }
  for (const auto& user : account->users) {
    DeleteUserAllowedQosOfAllPartition(GetUserInfo(user), qos, true);
  }
  account->allowed_qos_list.erase(iter);
  g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account, "$pull",
                               account->name, "allowed_qos_list", qos);
  return true;
}

bool AccountManager::DeleteAccountAllowedPartition(
    Ctld::Account* account, const std::string& partition) {
  auto iter = std::find(account->allowed_partition.begin(),
                        account->allowed_partition.end(), partition);
  if (iter == account->allowed_partition.end()) {
    return false;
  }

  for (const auto& child : account->child_account) {
    DeleteAccountAllowedPartition(GetAccountInfo(child), partition);
  }
  for (const auto& user : account->users) {
    DeleteUserAllowedPartition(GetUserInfo(user), partition);
  }
  account->allowed_partition.erase(iter);
  g_db_client->UpdateEntityOne(MongodbClient::EntityType::Account, "$pull",
                               account->name, "allowed_partition", partition);
  return true;
}

}  // namespace Ctld