#include "prometheus/registry.h"

#include <iterator>
#include <stdexcept>
#include <tuple>

namespace prometheus {

namespace {

bool FamilyNameExists(const std::string & /* name */) { return false; }

template <typename T, typename... Args>
bool FamilyNameExists(const std::string &name, const T &families,
                      Args &&... args) {
  auto sameName = [&name](const typename T::value_type &entry) {
    return name == entry->GetName();
  };
  auto exists = std::find_if(std::begin(families), std::end(families),
                             sameName) != std::end(families);
  return exists || FamilyNameExists(name, args...);
}

template <typename... Args>
void EnsureUniqueType(const std::string &name, Args &&... args) {
  if (FamilyNameExists(name, args...)) {
    throw std::invalid_argument("Family already exists with different type");
  }
}

template <typename T>
void CollectAll(std::vector<MetricFamily> &results, const T &families) {
  for (auto &&collectable : families) {
    auto metrics = collectable->Collect();
    results.insert(results.end(), std::make_move_iterator(metrics.begin()),
                   std::make_move_iterator(metrics.end()));
  }
}

}  // namespace

template <typename T>
Family<T> &Registry::Add(const std::string &name, const std::string &help,
                         const std::map<std::string, std::string> &labels,
                         std::vector<std::unique_ptr<Family<T>>> &families) {
  std::lock_guard<std::mutex> lock{mutex_};

  if (insert_behavior_ == InsertBehavior::Merge) {
    for (auto &family : families) {
      auto same = std::tie(name, labels) ==
                  std::tie(family->GetName(), family->GetConstantLabels());
      if (same) {
        return *family;
      }
    }
  }

  auto family = detail::make_unique<Family<T>>(name, help, labels);
  auto &ref = *family;
  families.push_back(std::move(family));
  return ref;
}

Family<Counter> &Registry::AddCounter(
    const std::string &name, const std::string &help,
    const std::map<std::string, std::string> &labels) {
  EnsureUniqueType(name, gauges_, histograms_, summaries_);
  return Add(name, help, labels, counters_);
}

Family<Gauge> &Registry::AddGauge(
    const std::string &name, const std::string &help,
    const std::map<std::string, std::string> &labels) {
  EnsureUniqueType(name, counters_, histograms_, summaries_);
  return Add(name, help, labels, gauges_);
}

Family<Histogram> &Registry::AddHistogram(
    const std::string &name, const std::string &help,
    const std::map<std::string, std::string> &labels) {
  EnsureUniqueType(name, counters_, gauges_, summaries_);
  return Add(name, help, labels, histograms_);
}

Family<Summary> &Registry::AddSummary(
    const std::string &name, const std::string &help,
    const std::map<std::string, std::string> &labels) {
  EnsureUniqueType(name, counters_, gauges_, histograms_);
  return Add(name, help, labels, summaries_);
}

std::vector<MetricFamily> Registry::Collect() {
  std::lock_guard<std::mutex> lock{mutex_};

  auto results = std::vector<MetricFamily>{};

  CollectAll(results, counters_);
  CollectAll(results, gauges_);
  CollectAll(results, histograms_);
  CollectAll(results, summaries_);

  return results;
}

}  // namespace prometheus
