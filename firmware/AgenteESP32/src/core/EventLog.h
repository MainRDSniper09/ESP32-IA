/**
 * @file EventLog.h
 * @brief Registro circular de eventos y alertas en RAM.
 *
 * Cada evento recibe un id creciente; los clientes web consultan
 * `GET /api/events?since=<id>` para recibir solo lo nuevo.
 * Las alertas (nivel warning/alert) quedan "sin reconocer" hasta que el
 * usuario las reconoce; mientras tanto el LED de alerta parpadea.
 */
#pragma once

#include <functional>
#include <vector>

#include "Types.h"

namespace agent {

class EventLog {
 public:
  typedef std::function<void(const Event&)> Listener;

  explicit EventLog(size_t capacity);

  /** Agrega un evento y notifica al listener (si existe). Devuelve su id. */
  uint32_t add(EventLevel level, const std::string& source, const std::string& message, time_t ts,
               uint32_t uptimeS);

  /** Copia los eventos con id > `sinceId` (como máximo `limit`, los más recientes). */
  std::vector<Event> since(uint32_t sinceId, size_t limit) const;

  /** Id del último evento (0 si no hay). */
  uint32_t lastId() const { return nextId_ - 1; }

  /** Alertas pendientes de reconocer. */
  uint32_t unacknowledged() const { return unacked_; }

  /** Marca todas las alertas como reconocidas. */
  void acknowledgeAll() { unacked_ = 0; }

  /** Función llamada en cada evento nuevo (debe ser rápida y no bloqueante). */
  void setListener(Listener l) { listener_ = l; }

 private:
  std::vector<Event> buffer_;
  size_t capacity_;
  size_t head_ = 0;  ///< Próxima posición a escribir
  size_t size_ = 0;
  uint32_t nextId_ = 1;
  uint32_t unacked_ = 0;
  Listener listener_;
};

}  // namespace agent
