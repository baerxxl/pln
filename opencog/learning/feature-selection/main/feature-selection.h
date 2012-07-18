/**
 * main/feature-selection.h --- 
 *
 * Copyright (C) 2011 OpenCog Foundation
 *
 * Author: Nil Geisweiller <nilg@desktop>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef _OPENCOG_FEATURE_SELECTION_H
#define _OPENCOG_FEATURE_SELECTION_H

#include <boost/assign/std/vector.hpp> // for 'operator+=()'
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/sort.hpp>
#include <boost/range/algorithm/binary_search.hpp>
#include <boost/range/algorithm/adjacent_find.hpp>
#include <boost/range/algorithm/set_algorithm.hpp>
#include <boost/range/irange.hpp>

#include <opencog/util/oc_omp.h>
#include <opencog/learning/moses/optimization/optimization.h>
#include <opencog/learning/moses/representation/field_set.h>
#include <opencog/learning/moses/representation/instance_set.h>
#include <opencog/learning/moses/moses/scoring.h>
#include <opencog/comboreduct/combo/table.h>

#include "../feature_scorer.h"
#include "../feature_max_mi.h"
#include "../feature_optimization.h"
#include "../moses_based_scorer.h"

using namespace opencog;
using namespace moses;
using namespace combo;
using namespace boost::assign; // bring 'operator+=()' into scope

// Feature selection algorithms
static const string un="un"; // moses based univariate
static const string sa="sa"; // moses based simulation annealing
static const string hc="hc"; // moses based hillclimbing
static const string inc="inc"; // incremental_selection (see
                               // feature_optimization.h)
static const string mmi="mmi"; // max_mi_selection (see
                               // feature_max_mi.h)

void err_empty_features() {
    std::cerr << "No features have been selected." << std::endl;
    exit(1);
}

// log the set of features and its number
void log_selected_features(arity_t old_arity, const Table& ftable) {
    // log the number selected features
    logger().info("%d out of %d have been selected",
                  ftable.get_arity(), old_arity);
    // log set of selected feature set
    stringstream ss;
    ss << "The following features have been selected: ";
    ostreamContainer(ss, ftable.itable.get_labels(), ",");
    logger().info(ss.str());
}

// parameters of feature-selection, see desc.add_options() in
// feature-selection.cc for their meaning
struct feature_selection_parameters
{
    std::string algorithm;
    unsigned int max_evals;
    std::string input_file;
    int target_feature;
    std::vector<int> ignore_features;
    std::vector<std::string> force_features_str;
    std::string output_file;
    unsigned target_size;
    double threshold;
    unsigned jobs;
    double inc_target_size_epsilon;
    double inc_red_intensity;
    unsigned inc_interaction_terms;
    double hc_max_score;
    double hc_confi; //  confidence intensity
    unsigned long hc_cache_size;
    double hc_fraction_of_remaining;
    std::vector<std::string> hc_initial_features;
};

typedef std::set<arity_t> feature_set;

template<typename Table, typename Optimize, typename Scorer>
feature_set moses_select_features(Table& table,
                                  const field_set& fields,
                                  instance_set<composite_score>& deme,
                                  instance& init_inst,
                                  Optimize& optimize, const Scorer& scorer,
                                  const feature_selection_parameters& fs_params)
{
    // optimize feature set
    unsigned ae; // actual number of evaluations to reached the best candidate
    unsigned evals = optimize(deme, init_inst, scorer, fs_params.max_evals, &ae);

    // get the best one
    boost::sort(deme, std::greater<scored_instance<composite_score> >());
    instance best_inst = evals > 0 ? *deme.begin_instances() : init_inst;
    composite_score best_score =
        evals > 0 ? *deme.begin_scores() : worst_composite_score;

    // get the best feature set
    feature_set selected_features = get_feature_set(fields, best_inst);
    // Logger
    {
        // log its score
        stringstream ss;
        ss << "Selected feature set has composite score: ";
        if (evals > 0)
            ss << best_score;
        else
            ss << "Unknown";
        logger().info(ss.str());
    }
    {
        // Log the actual number of evaluations
        logger().info("Total number of evaluations performed: %u", evals);
        logger().info("Actual number of evaluations to reach the best feature set: %u", ae);
    }
    // ~Logger
    return selected_features;
}

/** For the MOSES algo, generate the intial instance */
instance initial_instance(const feature_selection_parameters& fs_params,
                          const field_set& fields) {
    instance res(fields.packed_width());
    vector<std::string> labels = readInputLabels(fs_params.input_file,
                                                 fs_params.target_feature,
                                                 fs_params.ignore_features);
    vector<std::string> vif; // valid initial features, used for logging
    foreach(const std::string& f, fs_params.hc_initial_features) {
        size_t idx = std::distance(labels.begin(), boost::find(labels, f));
        if(idx < labels.size()) { // feature found
            *(fields.begin_bit(res) + idx) = true;
            // for logging
            vif += f;
        }
        else // feature not found
            logger().warn("No such a feature #%s in file %s. It will be ignored as initial feature.", f.c_str(), fs_params.input_file.c_str());
    }
    // Logger
    if(vif.empty())
        logger().info("The search will start with the empty feature set");
    else {
        stringstream ss;
        ss << "The search will start with the following feature set: ";
        ostreamContainer(ss, vif, ",");
        logger().info(ss.str());
    }
    // ~Logger
    return res;
}

// run feature selection given an moses optimizer
template<typename Optimize>
feature_set moses_select_features(Table& table,
                                  Optimize& optimize,
                                  const feature_selection_parameters& fs_params) {
    arity_t arity = table.get_arity();
    field_set fields(field_set::disc_spec(2), arity);
    instance_set<composite_score> deme(fields);
    // determine the initial instance given the initial feature set
    instance init_inst = initial_instance(fs_params, fields);
    // define feature set quality scorer
    typedef MICScorerTable<set<arity_t> > FSScorer;
    FSScorer fs_sc(table, fs_params.hc_confi);
    typedef moses_based_scorer<FSScorer> MBScorer;
    MBScorer mb_sc(fs_sc, fields);
    // possibly wrap in a cache
    if(fs_params.hc_cache_size > 0) {
        typedef prr_cache_threaded<MBScorer> ScorerCache;
        ScorerCache sc_cache(fs_params.hc_cache_size, mb_sc);
        feature_set selected_features =
            moses_select_features(table, fields, deme, init_inst, optimize,
                                  sc_cache, fs_params);
        // Logger
        logger().info("Number of cache failures = %u", sc_cache.get_failures());
        // ~Logger
        return selected_features;
    } else {
        return moses_select_features(table, fields, deme, init_inst, optimize,
                                     mb_sc, fs_params);
    }
}

/**
 * Add forced features to table.
 *
 * @todo update type_tree (if ever needed)
 */
Table add_force_features(const Table& table,
                         const feature_selection_parameters& fs_params) {
    const ITable& itable = table.itable;
    // get forced features that have not been selected
    std::vector<std::string> fnsel;
    const auto& ilabels = itable.get_labels();
    foreach(const std::string& fn, fs_params.force_features_str)
        if (boost::find(ilabels, fn) == ilabels.cend())
            fnsel.push_back(fn);

    // get their positions
    std::vector<int> fnsel_pos =
        find_features_positions(fs_params.input_file, fnsel);
    boost::sort(fnsel_pos);

    // get the complementary of their positions
    std::vector<int> fnsel_pos_comp;
    auto ir = boost::irange(0, dataFileArity(fs_params.input_file) + 1);
    boost::set_difference(ir, fnsel_pos, back_inserter(fnsel_pos_comp));

    // get header of the input table
    auto header = loadHeader(fs_params.input_file);
    
    // load the table with force_non_selected features with all
    // selected features with types definite_object (i.e. string) that
    // way the content is unchanged (convenient when the data contains
    // stuff that loadITable does not know how to interpret)
    ITable fns_itable;          // ITable from fnsel (+ output)
    type_tree tt = gen_signature(id::definite_object_type, fnsel.size());
    loadITable(fs_params.input_file, fns_itable, tt, fnsel_pos_comp);

    // Find the positions of the selected features
    std::vector<int> fsel_pos =
        find_features_positions(fs_params.input_file, ilabels);

    // insert the forced features in the right order
    Table new_table;
    new_table.otable = table.otable;
    new_table.itable = itable;
    // insert missing columns from fns_itable to new_table.itable
    for (auto lit = fnsel_pos.cbegin(), rit = fsel_pos.cbegin();
         lit != fnsel_pos.cend(); ++lit) {
        int lpos = distance(fnsel_pos.cbegin(), lit);
        auto lc = fns_itable.get_col(lpos);
        while(rit != fsel_pos.cend() && *lit > *rit) ++rit;
        int rpos = rit != fsel_pos.cend() ?
            distance(fsel_pos.cbegin(), rit) + lpos : -1;
        new_table.itable.insert_col(lc.first, lc.second, rpos);
    }

    return new_table;
}

/**
 * update fs_params.target_feature so that it keeps the same relative
 * position with the selected features.
 */
int update_target_feature(const Table& table,
                          const feature_selection_parameters& fs_params) {
    int tfp = fs_params.target_feature;
    if (tfp <= 0)               // it is either first or last
        return tfp;
    else {
        // Find the positions of the selected features
        std::vector<int> fsel_pos =
            find_features_positions(fs_params.input_file,
                                    table.itable.get_labels());
        if (tfp < fsel_pos.front()) // it is first
            return 0;
        else if (tfp > fsel_pos.back()) // it is last
            return -1;
        else {                  // it is somewhere in between
            auto it = boost::adjacent_find(fsel_pos, [tfp](int l, int r) {
                    return l < tfp && tfp < r; });
            return distance(fsel_pos.begin(), ++it);
        }
    }
}

void write_results(const Table& table,
                   const feature_selection_parameters& fs_params) {
    Table table_wff = add_force_features(table, fs_params);
    int tfp = update_target_feature(table_wff, fs_params);
    if(fs_params.output_file.empty())
        ostreamTable(std::cout, table_wff, tfp);
    else
        saveTable(fs_params.output_file, table_wff, tfp);
}

feature_set incremental_select_features(Table& table,
                                        const feature_selection_parameters& fs_params)
{
    auto ir = boost::irange(0, table.get_arity());
    feature_set all_features(ir.begin(), ir.end());
    if (fs_params.threshold > 0 || fs_params.target_size > 0) {
        CTable ctable = table.compress();
        typedef MutualInformation<feature_set> FeatureScorer;
        FeatureScorer fsc(ctable);
        return fs_params.target_size > 0?
            cached_adaptive_incremental_selection(all_features, fsc,
                                                  fs_params.target_size,
                                                  fs_params.inc_interaction_terms,
                                                  fs_params.inc_red_intensity,
                                                  0, 1,
                                                  fs_params.inc_target_size_epsilon)
            : cached_incremental_selection(all_features, fsc,
                                           fs_params.threshold,
                                           fs_params.inc_interaction_terms,
                                           fs_params.inc_red_intensity);
    } else {
        // Nothing happened, return all features by default
        return all_features;
    }
}

feature_set max_mi_select_features(Table& table,
                                   const feature_selection_parameters& fs_params)
{
    auto ir = boost::irange(0, table.get_arity());
    feature_set all_features(ir.begin(), ir.end());
    if (fs_params.target_size > 0) {
        CTable ctable = table.compress();
        typedef MutualInformation<feature_set> FeatureScorer;
        FeatureScorer fsc(ctable);
        return max_mi_selection(all_features, fsc,
                                (unsigned) fs_params.target_size,
                                fs_params.threshold);
    } else {
        // Nothing happened, return the all features by default
        return all_features;
    }
}

/**
 * Select the features according to the method described in fs_params.
 */
feature_set select_features(Table& table,
                            const feature_selection_parameters& fs_params) {
    if (fs_params.algorithm == moses::hc) {
        // setting moses optimization parameters
        double pop_size_ratio = 20;
        size_t max_dist = 4;
        score_t min_score_improv = 0.0;
        optim_parameters op_param(moses::hc, pop_size_ratio, fs_params.hc_max_score,
                                  max_dist, min_score_improv);
        op_param.hc_params = hc_parameters(true, // widen distance if no improvement
                                           false, // step (backward compatibility)
                                           false, // crossover
                                           fs_params.hc_fraction_of_remaining);
        hill_climbing hc(op_param);
        return moses_select_features(table, hc, fs_params);
    } else if (fs_params.algorithm == inc) {
        return incremental_select_features(table, fs_params);
    } else if (fs_params.algorithm == mmi) {
        return max_mi_select_features(table, fs_params);
    } else {
        std::cerr << "Fatal Error: Algorithm '" << fs_params.algorithm
                  << "' is unknown, please consult the help for the "
                     "list of algorithms." << std::endl;
        exit(1);
        return feature_set(); // to please Mr compiler
    }    
}

/**
 * Select the features, and output the table with the selected features
 */
void feature_selection(Table& table,
                       const feature_selection_parameters& fs_params)
{
    feature_set selected_features = select_features(table, fs_params);
    if (selected_features.empty())
        err_empty_features();
    else {
        Table ftable = table.filter(selected_features);
        log_selected_features(table.get_arity(), ftable);
        write_results(ftable, fs_params);
    }
}

#endif // _OPENCOG_FEATURE-SELECTION_H
