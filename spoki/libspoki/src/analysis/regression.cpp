/*
 * This file is part of the CAF spoki driver.
 *
 * Copyright (C) 2018-2021
 * Authors: Raphael Hiesgen
 *
 * All rights reserved.
 *
 * Report any bugs, questions or comments to raphael.hiesgen@haw-hamburg.de
 *
 */

#include "spoki/analysis/regression.hpp"

#include <cassert>

namespace spoki::analysis {

double mean(const std::vector<double>& xs) {
  auto sum = std::accumulate(std::begin(xs), std::end(xs), 0);
  return static_cast<double>(sum) / xs.size();
}

std::vector<double> pairwise_mult(const std::vector<double>& xs,
                                  const std::vector<double>& ys) {
  assert(xs.size() == ys.size());
  std::vector<double> xys(xs.size());
  for (size_t i = 0; i < xs.size(); ++i)
    xys[i] = xs[i] * ys[i];
  return xys;
}

// # Returns m and b for m*x + b
// def manual_best_fit(xs, ys):
//     xbar = sum(xs) / len(xs)
//     ybar = sum(ys) / len(ys)
//     n = len(xs) # or len(Y)
//     numer = sum([xi * yi for xi, yi in zip(xs, ys)]) - n * xbar * ybar
//     denum = sum([xi**2 for xi in xs]) - n * xbar**2
//     if denum == 0:
//         return 0, 0
//     m = numer / denum
//     b = ybar - m * xbar
//     # print('best fit line:\ny = {:.2f}x + {:.2f}'.format(m, b))
//     return m, b
std::pair<double, double> line_of_best_fit(const std::vector<double>& xs,
                                           const std::vector<double>& ys) {
  constexpr double epsilon = 0.00001;
  assert(xs.size() == ys.size());
  auto x_bar = mean(xs);
  auto y_bar = mean(ys);
  auto n = xs.size();
  double xs_squared_sum = 0;
  for (auto& x : xs)
    xs_squared_sum += x * x;
  auto dnum = xs_squared_sum - (n * x_bar * x_bar);
  if (dnum < epsilon)
    return std::make_pair(0.0, 0.0);
  double xys_sum = 0.0;
  for (size_t i = 0; i < xs.size(); ++i)
    xys_sum += xs[i] * ys[i];
  auto numer = xys_sum - (n * x_bar * y_bar);
  auto m = numer / dnum;
  auto b = y_bar - m * x_bar;
  return std::make_pair(m, b);
}

// # Probably the same as above ...
// def least_squares_method(xs, ys):
//     x_mean = get_mean(xs)
//     y_mean = get_mean(ys)
//     numer = sum([(x - x_mean) * (y - y_mean) for x, y, in zip(xs, ys)])
//     denum = sum([(x - x_mean) ** 2 for x in xs])
//     m = numer / denum
//     b = y_mean - m * x_mean
//     return m, b
//     # Alternative: get_correlation_coefficient(xs, ys) * get_std_dev(ys) /
//     get_std_dev(xs)
std::pair<double, double> least_squares_method(const std::vector<double>& xs,
                                               const std::vector<double>& ys) {
  assert(xs.size() == ys.size());
  auto x_bar = mean(xs);
  auto y_bar = mean(ys);
  double numer = 0.0;
  std::vector<double> numer_tmp(xs.size());
  for (size_t i = 0; i < xs.size(); ++i)
    numer += (xs[i] - x_bar) * (ys[i] - y_bar);
  double denum = 0.0;
  for (size_t i = 0; i < xs.size(); ++i)
    denum += (xs[i] - x_bar) * (xs[i] - x_bar);
  auto m = numer / denum;
  auto b = y_bar - m * x_bar;
  return std::make_pair(m, b);
}

// # gets variance n per default, setting ddof to 1 give variance n-1
// def get_variance(values, mean, ddof=0):
//     return sum([(v - mean) ** 2 for v in values]) / (len(values) - ddof)
double variance(const std::vector<double>& xs, double mean, unsigned ddof) {
  double res = 0.0;
  for (auto& x : xs)
    res += (x - mean) * (x - mean);
  return res / (xs.size() - ddof);
}

// def get_confidence(values, mean, ddof=0, z=1.96):
//     tmp = z * (get_std_dev(values, mean, ddof) / math.sqrt(len(values)))
//     return (mean - tmp), (mean + tmp)
std::pair<double, double> confidence(const std::vector<double>& xs, double mean,
                                     unsigned ddof, double z) {
  auto interval = z * (std_dev(xs, mean, ddof) / std::sqrt(xs.size()));
  return std::make_pair(mean - interval, mean + interval);
}

// # Cov: If this is greater one, there is a positive relationship between X and
// Y, # and if this is smaller than one, there is a negative relationship here.
// # Problem: This result depends on the magnitude or scale of measurment of the
// #  input, see the correlation coefficient for a better idicator.
// def get_covariance(x_vals, y_vals, ddof=1):
//     x_mean = get_mean(x_vals)
//     y_mean = get_mean(y_vals)
//     tmp = sum([(x - x_mean) * (y - y_mean) for x, y in zip(x_vals, y_vals)])
//     return tmp / (len(x_vals) - ddof)
double covariance(const std::vector<double>& xs, const std::vector<double>& ys,
                  unsigned ddof) {
  auto x_bar = mean(xs);
  auto y_bar = mean(ys);
  double tmp = 0.0;
  for (size_t i = 0; i < xs.size(); ++i)
    tmp += (xs[i] - x_bar) * (ys[i] - y_bar);
  return tmp / (xs.size() - ddof);
}

// # Cor: Standardized covariance, should be -1 <= Cor(X,Y) <= 1.
// # A Cor(X,Y) == 0 means that there is no linear relation here.
// def get_correlation_coefficient(x_vals, y_vals, ddof=1):
//     x_mean = get_mean(x_vals)
//     y_mean = get_mean(y_vals)
//     x_stdev = get_std_dev(x_vals, x_mean, ddof)
//     y_stdev = get_std_dev(y_vals, y_mean, ddof)
//     # Possibility 1
//     # tmp = sum([((x - x_mean) / x_stdev) * ((y - y_mean) / y_stdev) for x, y
//     in zip(x_vals, y_vals)]) # return tmp / (len(x_vals) - ddof) #
//     Possibility 2 tmp = get_covariance(x_vals, y_vals, ddof) return tmp /
//     (x_stdev * y_stdev) # Possibility 3 # tmp_top = sum([(x - x_mean) * (y -
//     y_mean) for x, y in zip(x_vals, y_vals)]) # part_x = sum([(x - x_mean) **
//     2 for x in x_vals]) # part_y = sum([(y - y_mean) ** 2 for y in y_vals])
//     # tmp_bot = math.sqrt(part_x * part_y)
//     # print("got: {} / sqrt({} * {})".format(tmp_top, part_x, part_y))
//     # return tmp_top / tmp_bot
double correlation_coefficient(const std::vector<double>& xs,
                               const std::vector<double>& ys, unsigned ddof) {
  auto x_bar = mean(xs);
  auto y_bar = mean(ys);
  auto x_stddev = std_dev(xs, x_bar, ddof);
  auto y_stddev = std_dev(ys, y_bar, ddof);
  auto tmp = covariance(xs, ys, ddof);
  return tmp / (x_stddev * y_stddev);
}

// # Also SSE or RSS. ... I think.
// def sum_squares_residual_errors(true_vals, predicted_vals):
//     return sum([(t - p) ** 2 for t, p in zip(true_vals, predicted_vals)])
double sum_squares_residual_errors(const std::vector<double>& measured,
                                   const std::vector<double>& predicted) {
  assert(measured.size() == predicted.size());
  double res = 0;
  for (size_t i = 0; i < measured.size(); ++i) {
    auto diff = (measured[i] - predicted[i]);
    res += diff * diff;
  }
  return res;
}

// # Used for prediction limits.
// def get_std_err_for_prediciton(x_vals, chosen_x, sigma):
//     x_mean = get_mean(x_vals)
//     sxx = sum([(x - x_mean) ** 2 for x in x_vals])
//     tmp = 1 + (1 / len(x_vals)) + (((chosen_x - x_mean) ** 2) / sxx)
//     return sigma * math.sqrt(tmp)
double std_err_for_prediciton(const std::vector<double>& xs, double chosen,
                              double sigma) {
  auto x_bar = mean(xs);
  double sxx = 0;
  for (auto& x : xs) {
    auto diff = x - x_bar;
    sxx += diff * diff;
  }
  auto diff = chosen - x_bar;
  double tmp = 1.0;
  tmp += (1.0 / xs.size());
  tmp += ((diff * diff) / sxx);
  return sigma * std::sqrt(tmp);
}

// def get_sigma_for_prediction(y_vals, predicted_vals, ddof=2):
//     return math.sqrt(sum_squares_residual_errors(y_vals, predicted_vals) /
//     (len(y_vals) - ddof))
double sigma_for_prediction(const std::vector<double>& actual,
                            const std::vector<double>& predicted,
                            unsigned ddof) {
  auto sse = sum_squares_residual_errors(actual, predicted);
  return std::sqrt(sse / (actual.size() - ddof));
}

// def get_standard_error_for_b(x_vals, sigma):
//    x_mean = get_mean(x_vals)
//    left = 1 / len(x_vals)
//    right = (x_mean ** 2) / sum([(x - x_mean) ** 2 for x in x_vals])
//    return sigma * math.sqrt(left + right)
double std_err_b(const std::vector<double>& xs, double sigma) {
  auto x_bar = mean(xs);
  auto left = static_cast<double>(1.0) / xs.size();
  double diff_sum = 0;
  for (auto& x : xs) {
    auto diff = x - x_bar;
    diff_sum += diff * diff;
  }
  auto right = (x_bar * x_bar) / diff_sum;
  return sigma * std::sqrt(left + right);
}

//# Esitmate of the the precision of the slope estimate. Smaller error means
// better estimate. def get_standard_error_for_m(x_vals, sigma):
//    x_mean = get_mean(x_vals)
//    tmp = sum([(x - x_mean) ** 2 for x in x_vals])
//    return sigma / math.sqrt(tmp)
double std_err_m(const std::vector<double>& xs, double sigma) {
  auto x_bar = mean(xs);
  double diff_sum = 0;
  for (auto& x : xs) {
    auto diff = x - x_bar;
    diff_sum += diff * diff;
  }
  return sigma / std::sqrt(diff_sum);
}

/*

# This seems wrong ...
# def get_prediction(values, mean, ddof=0, z=1.96):
#     tmp = z * get_std_dev(values, mean, ddof) * math.sqrt(1 + 1 /
len(values))) #     return (mean - tmp), (mean + tmp)
#
# def get_mean_squared_error(true_vals, predicted_val, ddof=2):
#     return sum([(t - p) ** 2 for t, p in zip(true_vals, predicted_val)]) /
(len(values) - ddof)
#
# def get_confidence_for_x(values, x, mean, ddof=0, z=1.96):
#     N = len(values)
#     math.sqrt(z*z + 1/N + (x - mean) ** 2 /(sum([(v - mean) ** 2 for v in
values])))

def standardize(vals, ddof=1):
    v_mean = get_mean(vals)
    stdev = get_std_dev(vals, v_mean, ddof)
    tmp = [(v - v_mean) / stdev for v in vals]
    return tmp

# Cor: Standardized covariance, should be -1 <= Cor(X,Y) <= 1.
# A Cor(X,Y) == 0 means that there is no linear relation here.
def get_correlation_coefficient(x_vals, y_vals, ddof=1):
    x_mean = get_mean(x_vals)
    y_mean = get_mean(y_vals)
    x_stdev = get_std_dev(x_vals, x_mean, ddof)
    y_stdev = get_std_dev(y_vals, y_mean, ddof)
    # Possibility 1
    # tmp = sum([((x - x_mean) / x_stdev) * ((y - y_mean) / y_stdev) for x, y in
zip(x_vals, y_vals)]) # return tmp / (len(x_vals) - ddof) # Possibility 2 tmp =
get_covariance(x_vals, y_vals, ddof) return tmp / (x_stdev * y_stdev) #
Possibility 3 # tmp_top = sum([(x - x_mean) * (y - y_mean) for x, y in
zip(x_vals, y_vals)]) # part_x = sum([(x - x_mean) ** 2 for x in x_vals]) #
part_y = sum([(y - y_mean) ** 2 for y in y_vals]) # tmp_bot = math.sqrt(part_x *
part_y) # print("got: {} / sqrt({} * {})".format(tmp_top, part_x, part_y)) #
return tmp_top / tmp_bot

# Followed this tutorial, what bugs me is that there is no distance involved.
# And the interval is the same for all values. A normal confidence interval
# differs, but also requires more than one measurement per value.
# https://machinelearningmastery.com/prediction-intervals-for-machine-learning/
def get_static_prediction_interval(true_vals, predicted_vals, ddof=2, z=1.96):
    # predicted_val +/- z * sigma
    sum_errs = sum([(y - yhat) ** 2 for y, yhat in zip(true_vals,
predicted_vals)]) stdev = math.sqrt(1 / (len(true_vals) - ddof) * sum_errs)
    interval = z * stdev
    # print("true: {}".format(true_vals))
    # print("pred: {}".format(predicted_vals))
    # print("sum:  {}".format(sum_errs))
    # print("std dev: {}".format(stdev))
    # print("interval: {}".format(interval))
    return interval

# Also SSE or RSS. ... I think.
def sum_squares_residual_errors(true_vals, predicted_vals):
    return sum([(t - p) ** 2 for t, p in zip(true_vals, predicted_vals)])

def get_sigma_for_prediction(y_vals, predicted_vals, ddof=2):
    return math.sqrt(sum_squares_residual_errors(y_vals, predicted_vals) /
(len(y_vals) - ddof))

def get_standard_error_for_b(x_vals, sigma):
    x_mean = get_mean(x_vals)
    left = 1 / len(x_vals)
    right = (x_mean ** 2) / sum([(x - x_mean) ** 2 for x in x_vals])
    return sigma * math.sqrt(left + right)

# Esitmate of the the precision of the slope estimate. Smaller error means
better estimate. def get_standard_error_for_m(x_vals, sigma): x_mean =
get_mean(x_vals) tmp = sum([(x - x_mean) ** 2 for x in x_vals]) return sigma /
math.sqrt(tmp)

# Used for prediction limits.
def get_std_err_for_prediciton(x_vals, chosen_x, sigma):
    x_mean = get_mean(x_vals)
    sxx = sum([(x - x_mean) ** 2 for x in x_vals])
    tmp = 1 + (1 / len(x_vals)) + (((chosen_x - x_mean) ** 2) / sxx)
    return sigma * math.sqrt(tmp)

# Not sure what this did ...
# def get_std_err_for_response(x_vals, chosen_x, sigma):
    # x_mean = get_mean(x_vals)
    # sxx = math.sqrt([(x - x_mean) ** 2 for x in x_vals])
    # tmp = (1 / len(x_vals)) + ((chosen_x - x_mean) ** 2 / sxx)
    # return sigma * math.sqrt(tmp)

# It sounds like this is the mean of values for a specific prediciton?
def get_prediciton_fit(x_vals, chosen_x, sigma):
    x_mean = get_mean(x_vals)
    sxx = math.sqrt([(x - x_mean) ** 2 for x in x_vals])
    tmp = (1 / len(x_vals)) + ((chosen_x - x_mean) ** 2 / sxx)
    return sigma * math.sqrt(tmp)
*/

} // namespace spoki::analysis
