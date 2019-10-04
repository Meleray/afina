#include "SimpleLRU.h"
#define node it->second.get()

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
  if (key.size() + value.size() > _max_size) {
    return false;
  }
  auto it = _lru_index.find(key);
  if (it != _lru_index.end()) {
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
    size += value.size() - node.value.size();
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
    node.value = value;
  }
  else
  {
    size += value.size() + key.size();
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
    while (size + value.size() + key.size() > _max_size) {
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
    size += value.size() + key.size();
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
    size += value.size() - node.value.size();
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
    value = node.value;
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
    return true;
  }
  else
  {
    return false;
  }
}

} // namespace Backend
} // namespace Afina
