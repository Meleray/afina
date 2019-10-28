#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

void SimpleLRU::Tohead(lru_node & node) {
  if (node.key != _lru_head->key) {
    auto tmp = std::move(node.prev->next);
    if (node.next) {
      node.next->prev = node.prev;
    }
    else
    {
      _lru_tail = node.prev;
    }
    node.prev->next = std::move(node.next);
    node.prev = NULL;
    _lru_head->prev = &node;
    node.next = std::move(_lru_head);
    _lru_head = std::move(tmp);
  }
}

void SimpleLRU::Free_space() {
  while (size > _max_size) {
    size -= _lru_tail->key.size() + _lru_tail->value.size();
    _lru_index.erase(_lru_tail->key);
    _lru_tail = _lru_tail->prev;
    if (_lru_tail) {
      _lru_tail->next = NULL;
    }
    else {
      _lru_head = NULL;
    }
  }
}

void SimpleLRU::Create_node(const std::string &key, const std::string &value) {
  lru_node *curr = new lru_node;
  curr->value = value;
  curr->key = key;
  curr->prev = NULL;
  if (_lru_head) {
    _lru_head->prev = curr;
  }
  curr->next = std::move(_lru_head);
  _lru_head = std::unique_ptr<lru_node>(curr);
  if (!_lru_tail) {
    _lru_tail = curr;
  }
  _lru_index.insert(std::make_pair(std::ref(curr->key), std::ref(*curr)));
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
  if (key.size() + value.size() > _max_size) {
    return false;
  }
  auto it = _lru_index.find(key);
  if (it != _lru_index.end()) {
    lru_node &node = it->second.get();
    Tohead(node);
    size += value.size() - node.value.size();
    Free_space();
    node.value = value;
  }
  else
  {
    size += value.size() + key.size();
    Free_space();
    Create_node(key, value);
  }
  return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value)
{
  if (_lru_index.find(key) != _lru_index.end()) {
    return false;
  }
  else
  {
    if (value.size() + key.size() > _max_size){
      return false;
    }
    size += value.size() + key.size();
    Free_space();
    Create_node(key, value);
    return true;
  }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value)
{
  auto it = _lru_index.find(key);
  if (it != _lru_index.end()) {
    if (value.size() + key.size() > _max_size){
      return false;
    }
    lru_node &node = it->second.get();
    Tohead(node);
    size += value.size() - node.value.size();
    Free_space();
    node.value = value;
    return true;
  }
  else
  {
    return false;
  }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key)
{
  auto it = _lru_index.find(key);
  if (it != _lru_index.end()) {
    lru_node &node = it->second.get();
    size -= key.size() + node.value.size();
    if (node.next) {
      node.next->prev = node.prev;
    }
    else
    {
      _lru_tail = node.prev;
    }
    if (node.prev) {
      node.prev->next = std::move(node.next);
    }
    else
    {
      _lru_head = std::move(node.next);
    }
    _lru_index.erase(it);
    return true;
  }
  else
  {
    return false;
  }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value)
{
  auto it = _lru_index.find(key);
  if (it != _lru_index.end()) {
    lru_node &node = it->second.get();
    value = node.value;
    Tohead(node);
    return true;
  }
  else
  {
    return false;
  }
}

} // namespace Backend
} // namespace Afina
