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

#pragma once

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "spoki/detail/core_export.hpp"

namespace spoki::analysis {

// clang-format off
/*

"""
# Table for Student's t value, from https://www.thoughtco.com/student-t-distribution-table-3126265
# Used for prediction interval calculation and hypothesis testing.
# Left column: degrees of freedom (here: sample size - 1)
# Top row:     confidence inerval alpha / 2
# TODO: pack this into a data structure.
STUDENTS_T = {
        0.40        0.25        0.10        0.05        0.025       0.01        0.005       0.0005
    1   0.324920    1.000000    3.077684    6.313752   12.70620    31.82052    63.65674   636.6192
    2   0.288675    0.816497    1.885618    2.919986    4.30265     6.96456     9.92484    31.5991
    3   0.276671    0.764892    1.637744    2.353363    3.18245     4.54070     5.84091    12.9240
    4   0.270722    0.740697    1.533206    2.131847    2.77645     3.74695     4.60409     8.6103
    5   0.267181    0.726687    1.475884    2.015048    2.57058     3.36493     4.03214     6.8688
    6   0.264835    0.717558    1.439756    1.943180    2.44691     3.14267     3.70743     5.9588
    7   0.263167    0.711142    1.414924    1.894579    2.36462     2.99795     3.49948     5.4079
    8   0.261921    0.706387    1.396815    1.859548    2.30600     2.89646     3.35539     5.0413
    9   0.260955    0.702722    1.383029    1.833113    2.26216     2.82144     3.24984     4.7809
    10  0.260185    0.699812    1.372184    1.812461    2.22814     2.76377     3.16927     4.5869
    11  0.259556    0.697445    1.363430    1.795885    2.20099     2.71808     3.10581     4.4370
    12  0.259033    0.695483    1.356217    1.782288    2.17881     2.68100     3.05454     4.3178
    13  0.258591    0.693829    1.350171    1.770933    2.16037     2.65031     3.01228     4.2208
    14  0.258213    0.692417    1.345030    1.761310    2.14479     2.62449     2.97684     4.1405
    15  0.257885    0.691197    1.340606    1.753050    2.13145     2.60248     2.94671     4.0728
    16  0.257599    0.690132    1.336757    1.745884    2.11991     2.58349     2.92078     4.0150
    17  0.257347    0.689195    1.333379    1.739607    2.10982     2.56693     2.89823     3.9651
    18  0.257123    0.688364    1.330391    1.734064    2.10092     2.55238     2.87844     3.9216
    19  0.256923    0.687621    1.327728    1.729133    2.09302     2.53948     2.86093     3.8834
    20  0.256743    0.686954    1.325341    1.724718    2.08596     2.52798     2.84534     3.8495
    21  0.256580    0.686352    1.323188    1.720743    2.07961     2.51765     2.83136     3.8193
    22  0.256432    0.685805    1.321237    1.717144    2.07387     2.50832     2.81876     3.7921
    23  0.256297    0.685306    1.319460    1.713872    2.06866     2.49987     2.80734     3.7676
    24  0.256173    0.684850    1.317836    1.710882    2.06390     2.49216     2.79694     3.7454
    25  0.256060    0.684430    1.316345    1.708141    2.05954     2.48511     2.78744     3.7251
    26  0.255955    0.684043    1.314972    1.705618    2.05553     2.47863     2.77871     3.7066
    27  0.255858    0.683685    1.313703    1.703288    2.05183     2.47266     2.77068     3.6896
    28  0.255768    0.683353    1.312527    1.701131    2.04841     2.46714     2.76326     3.6739
    29  0.255684    0.683044    1.311434    1.699127    2.04523     2.46202     2.75639     3.6594
    30  0.255605    0.682756    1.310415    1.697261    2.04227     2.45726     2.75000     3.6460
}
"""

*/
// clang-format on

/// Returns the mean value for a given vector.
SPOKI_CORE_EXPORT double mean(const std::vector<double>& xs);

/// Pairwise multiply the elements in two vectors. Should probably be an
/// iterator interface.
SPOKI_CORE_EXPORT std::vector<double>
pairwise_mult(const std::vector<double>& xs, const std::vector<double>& ys);

/// Calculates the line of best fit for the x and y values.
/// Returns (m, b) as a pair for m * x + b.
SPOKI_CORE_EXPORT std::pair<double, double>
line_of_best_fit(const std::vector<double>& xs, const std::vector<double>& ys);

/// Calculate the line of best fit using the least squares method.
/// Returns (m, b) for
SPOKI_CORE_EXPORT std::pair<double, double>
least_squares_method(const std::vector<double>& xs,
                     const std::vector<double>& ys);

/// Calculates the variance for given values `xs` with `mean` for `ddof` degrees
/// of freedom.
SPOKI_CORE_EXPORT double variance(const std::vector<double>& xs, double mean,
                                  unsigned ddof = 0);

/// Calculates the standard deviation for the values `xs` with `mean` and `ddof`
/// degrees of freedom.
inline double std_dev(const std::vector<double>& xs, double mean,
                      unsigned ddof = 0) {
  // def get_std_dev(values, mean, ddof=0):
  //     return math.sqrt(get_variance(values, mean, ddof))
  return std::sqrt(variance(xs, mean, ddof));
}

/// Calculates the confidence interval for `xs` with `mean`, `ddof` degrees of
/// freedom and a given z value. The vlaue for z can be looked up the in the
/// prediction interval table below.
///
/// Percentage | Multiplier z
/// -----------+-------------
///         50 |  0.67
///         55 |  0.76
///         60 |  0.84
///         65 |  0.93
///         70 |  1.04
///         75 |  1.15
///         80 |  1.28
///         85 |  1.44
///         95 |  1.96
///         90 |  1.64
///         96 |  2.05
///         97 |  2.17
///         98 |  2.33
///         99 |  2.58
SPOKI_CORE_EXPORT std::pair<double, double>
confidence(const std::vector<double>& xs, double mean, unsigned ddof = 0,
           double z = 1.96);

/// Calculates the covariance for the given coordiantes `xs` and `ys` with
/// `ddof` degrees of freedom. If this is greater one, there is a positive
/// relationship between X and Y, and if this is smaller than one, there is a
/// negative relationship here. Problem: This result depends on the magnitude or
/// scale of measurment of the input, see the correlation coefficient for a
/// better indicator.
SPOKI_CORE_EXPORT double covariance(const std::vector<double>& xs,
                                    const std::vector<double>& ys,
                                    unsigned ddof = 1);

/// Calculates the correlation coefficient for the given coordiantes `xs` and
/// `ys` with `ddof` degrees of freedom. If this is greater one, there is a
/// positive relationship between X and Y, and if this is smaller than one,
/// there is a negative relationship here.
/// This is a standardized covariance, should be -1 <= Cor(X,Y) <= 1.
SPOKI_CORE_EXPORT double correlation_coefficient(const std::vector<double>& xs,
                                                 const std::vector<double>& ys,
                                                 unsigned ddof = 1);

/// Also called SSE or RSS. (... I think).
SPOKI_CORE_EXPORT double
sum_squares_residual_errors(const std::vector<double>& measured,
                            const std::vector<double>& predicted);

/// Used to calcualte the prediction interval.
SPOKI_CORE_EXPORT double std_err_for_prediciton(const std::vector<double>& xs,
                                                double chosen, double sigma);

/// Used to calculate the prediction interval.
SPOKI_CORE_EXPORT double
sigma_for_prediction(const std::vector<double>& actual,
                     const std::vector<double>& predicted, unsigned ddof = 2);

SPOKI_CORE_EXPORT double std_err_b(const std::vector<double>& xs, double sigma);

SPOKI_CORE_EXPORT double std_err_m(const std::vector<double>& xs, double sigma);

/// Calculates the prediciton interval for a given standard error with `t`.
/// `t` depends on the degrees of freedom, can be looked up in the Student's
/// t-score table, degrees of freedom are the sample size - 1 in this case.
/// For now let's use 3 degrees of freedom with alpha = 0.025 (95% conf
/// interval) as a default.
inline double prediction_interval(double std_err, double t = 3.18245) {
  // def get_prediction_interval(stderr, t=3.18):
  //     interval = t * stderr
  //     return interval
  return t * std_err;
}

} // namespace spoki::analysis
