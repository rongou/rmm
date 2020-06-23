/*
 * Copyright (c) 2020, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either ex  ess or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <thrust/iterator/zip_iterator.h>
#include <memory>
#include <stdexcept>
#include <string>
#include "rapidcsv.h"

namespace rmm {
namespace detail {

enum class action : bool { ALLOCATE, FREE };

/**
 * @brief Represents an allocation event
 *
 */
struct event {
  event()             = default;
  event(event const&) = default;
  event(action a, std::size_t s, void const* p)
    : act{a}, size{s}, pointer{reinterpret_cast<uintptr_t>(p)}
  {
  }

  event(action a, std::size_t s, uintptr_t p) : act{a}, size{s}, pointer{p} {}

  action act{};         ///< Indicates if the event is an allocation or a free
  std::size_t size{};   ///< The size of the memory allocated or free'd
  uintptr_t pointer{};  ///< The pointer returned from an allocation, or the
                        ///< pointer free'd
};

bool operator==(event const& lhs, event const& rhs)
{
  return std::tie(lhs.act, lhs.size, lhs.pointer) == std::tie(rhs.act, rhs.size, rhs.pointer);
}

/**
 * @brief Parses a RMM log file into a vector of events
 *
 * Parses a log file generated from `rmm::mr::logging_resource_adaptor` into a vector of `event`s.
 * An `event` describes an allocation/deallocation event that occurred via the logging adaptor.
 *
 * @param filename Name of the RMM log file
 * @return Vector of events from the contents of the log file
 */
std::vector<event> parse_csv(std::string const& filename)
{
  rapidcsv::Document csv(filename);

  std::vector<std::string> actions  = csv.GetColumn<std::string>("Action");
  std::vector<std::size_t> sizes    = csv.GetColumn<std::size_t>("Size");
  std::vector<std::string> pointers = csv.GetColumn<std::string>("Pointer");

  if ((sizes.size() != actions.size()) or (sizes.size() != pointers.size())) {
    throw std::runtime_error{"Size mismatch in actions, sizes, or pointers."};
  }

  std::vector<event> events(sizes.size());

  auto zipped_begin =
    thrust::make_zip_iterator(thrust::make_tuple(actions.begin(), sizes.begin(), pointers.begin()));
  auto zipped_end = zipped_begin + sizes.size();

  std::transform(zipped_begin,
                 zipped_end,
                 events.begin(),
                 [](thrust::tuple<std::string, std::size_t, std::string> const& t) {
                   // Convert "allocate" or "free" string into `action` enum
                   action a = (thrust::get<0>(t) == "allocate") ? action::ALLOCATE : action::FREE;
                   std::size_t size = thrust::get<1>(t);

                   // Convert pointer string into an integer
                   uintptr_t p = std::stoll(thrust::get<2>(t), nullptr, 16);
                   return event{a, size, p};
                 });

  return events;
}

}  // namespace detail
}  // namespace rmm