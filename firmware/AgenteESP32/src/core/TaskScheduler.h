/**
 * @file TaskScheduler.h
 * @brief Lista de tareas programadas pendientes (sin hilos, sin bloqueos).
 *
 * El planificador NO usa delay(): el lazo de E/S llama a `takeDue(now)` cada
 * AGENT_SCHEDULER_TICK_MS y recibe las tareas cuyo instante ya llegó. Así el
 * monitoreo de puertas/ventanas nunca se detiene mientras hay tareas en espera.
 *
 * La lista se serializa a JSON para guardarse en memoria no volátil:
 * {
 *   "next_id": 7,
 *   "tasks": [ {"id":3,"due":1762477200,"created":1762470000,
 *               "repeat":"daily","origin":"texto...","step":{...}} ]
 * }
 * El campo "step" usa el mismo formato que el LLM y se re-valida al cargar.
 */
#pragma once

#include <string>
#include <vector>

#include "Types.h"

namespace agent {

class TaskScheduler {
 public:
  explicit TaskScheduler(size_t capacity) : capacity_(capacity) {}

  /** Espacio libre. */
  size_t freeSlots() const { return tasks_.size() >= capacity_ ? 0 : capacity_ - tasks_.size(); }
  size_t size() const { return tasks_.size(); }
  size_t capacity() const { return capacity_; }

  /** Agrega una tarea y devuelve su id (0 si no hay espacio). */
  uint32_t add(const PlanStep& step, time_t due, time_t createdAt, const std::string& origin);

  /** Cancela por id. */
  bool cancel(uint32_t id);

  /** Cancela todas y devuelve cuántas había. */
  size_t cancelAll();

  /** Busca una tarea por id (nullptr si no existe). */
  const ScheduledTask* find(uint32_t id) const;

  /** Tareas ordenadas por instante de ejecución. */
  std::vector<ScheduledTask> list() const;

  /**
   * Extrae las tareas vencidas (due <= now). Las tareas diarias se
   * reprograman para el día siguiente en vez de eliminarse.
   */
  std::vector<ScheduledTask> takeDue(time_t now);

  /** Serializa a JSON (ver formato arriba). */
  std::string serialize() const;

  /**
   * Carga desde JSON. Las entradas inválidas se descartan y se cuentan en
   * `discarded`. Devuelve false si el documento completo es ilegible.
   */
  bool deserialize(const std::string& json, size_t& discarded);

 private:
  std::vector<ScheduledTask> tasks_;
  size_t capacity_;
  uint32_t nextId_ = 1;
};

}  // namespace agent
