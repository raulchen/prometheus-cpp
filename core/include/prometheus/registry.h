#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "prometheus/collectable.h"
#include "prometheus/counter.h"
#include "prometheus/detail/counter_builder.h"
#include "prometheus/detail/future_std.h"
#include "prometheus/detail/gauge_builder.h"
#include "prometheus/detail/histogram_builder.h"
#include "prometheus/detail/summary_builder.h"
#include "prometheus/family.h"
#include "prometheus/gauge.h"
#include "prometheus/histogram.h"
#include "prometheus/metric_family.h"
#include "prometheus/summary.h"

namespace prometheus {

/// \brief Manages the collection of a number of metrics.
///
/// The Registry is responsible to expose data to a class/method/function
/// "bridge", which returns the metrics in a format Prometheus supports.
///
/// The key class is the Collectable. This has a method - called Collect() -
/// that returns zero or more metrics and their samples. The metrics are
/// represented by the class Family<>, which implements the Collectable
/// interface. A new metric is registered with BuildCounter(), BuildGauge(),
/// BuildHistogram() or BuildSummary().
///
/// The class is thread-safe. No concurrent call to any API of this type causes
/// a data race.
class Registry : public Collectable {
 public:
  /// \brief How to deal with repeatedly added family names for a type
  enum class InsertBehavior {
    /// \brief Create new family object and append
    Append,
    /// \brief Merge with existing ones if possible
    Merge,
  };

  /// \brief name Create a new registry.
  ///
  /// \param insert_behavior How to handle families with the same name.
  explicit Registry(InsertBehavior insert_behavior = InsertBehavior::Append)
      : insert_behavior_{insert_behavior} {}

  /// \brief Returns a list of metrics and their samples.
  ///
  /// Every time the Registry is scraped it calls each of the metrics Collect
  /// function.
  ///
  /// \return Zero or more metrics and their samples.
  std::vector<MetricFamily> Collect() override;

 private:
  friend class detail::CounterBuilder;
  friend class detail::GaugeBuilder;
  friend class detail::HistogramBuilder;
  friend class detail::SummaryBuilder;

  template <typename T>
  Family<T>& Add(const std::string& name, const std::string& help,
                 const std::map<std::string, std::string>& labels,
                 std::vector<std::unique_ptr<Family<T>>>& families);

  Family<Counter>& AddCounter(const std::string& name, const std::string& help,
                              const std::map<std::string, std::string>& labels);

  Family<Gauge>& AddGauge(const std::string& name, const std::string& help,
                          const std::map<std::string, std::string>& labels);

  Family<Histogram>& AddHistogram(
      const std::string& name, const std::string& help,
      const std::map<std::string, std::string>& labels);

  Family<Summary>& AddSummary(const std::string& name, const std::string& help,
                              const std::map<std::string, std::string>& labels);

  const InsertBehavior insert_behavior_;
  std::vector<std::unique_ptr<Family<Counter>>> counters_;
  std::vector<std::unique_ptr<Family<Gauge>>> gauges_;
  std::vector<std::unique_ptr<Family<Histogram>>> histograms_;
  std::vector<std::unique_ptr<Family<Summary>>> summaries_;
  std::mutex mutex_;
};

}  // namespace prometheus
