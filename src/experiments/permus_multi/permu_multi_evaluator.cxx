#include "std.h" // Must be included first. Precompiled header with standard library includes.
#include "std.hxx"
#include "permuevaluator.h"
#include "FitnessFunction_permu.h"
#include "map.h"
#include "network.h"
#include "networkexecutor.h"
#include "resource.h"
#include <assert.h>
#include "Population.h"
#include "Tools.h"
#include "PBP.h"
#include <cfloat>
#include "INIReader.h"
#include "loadnetworkfromfile.h"
#include <omp.h>
#include "Population.h"
#include "experiment.h"
#include "neat.h"
#include "rng.h"
#include "util.h"
#include <iomanip>
#include "Parameters.h"
#include "permuevaluator.h"
#include "PERMU_MULTI_params.h"
#include "permu_multi_evaluator.h"
#include <vector>

using namespace std;

//#define COUNTER
//#define PRINT
//#define RANDOM_SEARCH

namespace PERMU_MULTI
{

void convert_f_values_to_ranks(vector<int> surviving_candidates, double **f_values, double **ranks, int current_n_of_evals)
{
    // replace f values with ranks
    static vector<vector<double>> ranks_vec;
    double *f_values_surviving_candidates[surviving_candidates.size()]; 
    ranks_vec.resize(surviving_candidates.size());
    for (int i = 0; i < surviving_candidates.size(); i++)
    {
        f_values_surviving_candidates[i] = f_values[surviving_candidates[i]];
        ranks_vec[i].resize(current_n_of_evals);
    }

    get_ranks_from_f_values(ranks_vec, f_values_surviving_candidates, surviving_candidates.size(), current_n_of_evals);
    for (int i = 0; i < surviving_candidates.size(); i++)
    {
        for (int j = 0; j < current_n_of_evals; j++)
        {
            ranks[surviving_candidates[i]][j] = ranks_vec[i][j];
        }
    }
}



struct Evaluator
{

    PERMU_MULTI::params_multi *parameters;
    NEAT::CpuNetwork *best_network;
    int iteration_number = 0;

    __net_eval_decl Evaluator(){};

    // fitness function in sequential order
    __net_eval_decl double FitnessFunction(NEAT::CpuNetwork *net, int initial_seed, int instance_index)
    {
        int seed_seq = initial_seed;
        PERMU_MULTI::params_multi tmp_params = *parameters;
        tmp_params.INSTANCE_PATH = (*parameters->VECTOR_OF_INSTANCE_PATHS)[instance_index];
        double res = FitnessFunction_permu(net, seed_seq, &tmp_params);
        return res;
    }


    // compute the fitness value of all networks at training time.
    __net_eval_decl void execute(class NEAT::Network **nets_, NEAT::OrganismEvaluation *results, size_t nnets)
    {


        

        int n_instances = (*this->parameters->VECTOR_OF_INSTANCE_PATHS).size();
        int n_reps_all_instances = EVAL_MIN_STEP / n_instances;
        int n_evals_each_it = n_reps_all_instances * n_instances;

        if (n_instances == 0)
        {
            cout << "n_instances = 0 in permu_multi_evaluator.cxx" << endl;
            exit(1);
        }



        using namespace PERMU;
        NEAT::CpuNetwork **nets = (NEAT::CpuNetwork **)nets_;
        double **f_values = new double *[nnets+1];
        double **f_value_ranks = new double *[nnets+1];

        RandomNumberGenerator rng;

        for (size_t i = 0; i < nnets+1; i++)
        {
            f_values[i] = new double[EVALS_TO_SELECT_BEST_CONTROLLER_IN_LAST_IT + EVAL_MIN_STEP + n_evals_each_it];
            f_value_ranks[i] = new double[EVALS_TO_SELECT_BEST_CONTROLLER_IN_LAST_IT + EVAL_MIN_STEP + n_evals_each_it];
        }

        double *tmp_order = new double[nnets+1];
        // evaluate the individuals

        if (best_network == nullptr)
        {
            best_network = new NEAT::CpuNetwork(*nets[0]);
        }


        int current_n_of_evals = 0;
        int target_n_controllers_left;
        int max_evals_per_controller;
        int ALPHA_INDEX;

        vector<int> surviving_candidates;

        for (int inet = 0; inet < nnets; inet++)
        {
            surviving_candidates.push_back(inet);
        }


        if (!parameters->neat_params->IS_LAST_ITERATION)
        {
            target_n_controllers_left = (int)((double)nnets * NEAT::env->survival_thresh);
            max_evals_per_controller = MAX_EVALS_PER_CONTROLLER_NEUROEVOLUTION;
            ALPHA_INDEX = 0;
        }
        else
        {
            target_n_controllers_left = 1;
            max_evals_per_controller = EVALS_TO_SELECT_BEST_CONTROLLER_IN_LAST_IT;
            ALPHA_INDEX = 1;
        }
        




        while (surviving_candidates.size() > target_n_controllers_left && max_evals_per_controller > current_n_of_evals)
        {
            int initial_seed = rng.random_integer_uniform(INT_MAX);
            cout << "Evaluating -> " << std::flush;
            //cout << endl << "inet" << "," << "f_value_sample_index" << "," << "instance_index" << "," << "seed" << endl;

            int n_surviving_candidates = surviving_candidates.size();
            progress_bar bar(n_surviving_candidates* n_evals_each_it);
            #pragma omp parallel for num_threads(parameters->neat_params->N_OF_THREADS)
            for (int i = 0; i < n_surviving_candidates * n_evals_each_it; i++)
            {
                int inet = surviving_candidates[i % n_surviving_candidates];
                int f_value_sample_index = i / n_surviving_candidates;
                int instance_index =  f_value_sample_index % n_instances;

                NEAT::CpuNetwork *net = nets[inet];
                int seed = initial_seed + i / n_surviving_candidates;

                //cout << inet << "," << f_value_sample_index << "," << instance_index << "," << seed << endl;


                f_values[inet][f_value_sample_index] = this->FitnessFunction(net, seed, instance_index);
                bar.step();
            }

            bar.end();
            cout << ", ";
            current_n_of_evals += n_evals_each_it;

            convert_f_values_to_ranks(surviving_candidates, f_values, f_value_ranks, current_n_of_evals);


            for (auto &&inet : surviving_candidates)
            {
                tmp_order[inet] = Average(f_value_ranks[inet], current_n_of_evals) - (double)surviving_candidates.size() * 10000000.0;
            }



            F_race_iteration(f_value_ranks, surviving_candidates, current_n_of_evals, ALPHA_INDEX);


            cout << ", perc_discarded: " << (double)(nnets - surviving_candidates.size()) / (double)(nnets);
            cout << ", " << surviving_candidates.size() << " left.";
            cout << endl;
        }


        // update best known of last iteration
        int initial_seed = rng.random_integer_uniform(INT_MAX);
        #pragma omp parallel for num_threads(parameters->neat_params->N_OF_THREADS)
        for (int i = 0; i < current_n_of_evals; i++)
        {
            f_values[nnets][i] = this->FitnessFunction(best_network, initial_seed + i, i % n_instances);
        }
        surviving_candidates.push_back((int) nnets);
        convert_f_values_to_ranks(surviving_candidates, f_values, f_value_ranks, current_n_of_evals);
        double avg_perf_best = Average(f_value_ranks[nnets], current_n_of_evals);
        parameters->neat_params->BEST_FITNESS_TRAIN = (avg_perf_best + parameters->neat_params->BEST_FITNESS_TRAIN) / 2;

        for (auto &&inet : surviving_candidates)
        {
            tmp_order[inet] = Average(f_value_ranks[inet], current_n_of_evals) - (double)surviving_candidates.size() * 10000000.0;
        }

        double best_f_gen = Average(f_value_ranks[argmax(tmp_order, (int)nnets)], current_n_of_evals);
        cout << "(best this gen, best last gen) -> (" << best_f_gen << ", " << parameters->neat_params->BEST_FITNESS_TRAIN << ")";

        if (best_f_gen > parameters->neat_params->BEST_FITNESS_TRAIN || parameters->neat_params->IS_LAST_ITERATION)
        {
            parameters->neat_params->N_TIMES_BEST_FITNESS_IMPROVED_TRAIN++;
            cout << ", best replaced";
            parameters->neat_params->BEST_FITNESS_TRAIN = best_f_gen;
            delete best_network;
            best_network = new NEAT::CpuNetwork(*nets[argmax(tmp_order, (int)nnets)]);
            tmp_order[argmax(tmp_order, (int)nnets)] += 10000000.0;
        }

        cout << endl;



        compute_order_from_double_to_double(tmp_order, nnets, tmp_order, false, true);



        multiply_array_with_value(tmp_order, 1.0 / (double)(nnets - 1), (int)nnets);
        multiply_array_with_value(tmp_order, 1.0 + ((double)parameters->neat_params->N_TIMES_BEST_FITNESS_IMPROVED_TRAIN / 1000.0), (int)nnets);


        if (parameters->neat_params->IS_LAST_ITERATION)
        {

            cout << "fitness_matrix: " << std::flush;
            PrintMatrix(f_values, nnets, n_evals_each_it);

            cout << "franks_matrix (it is normal that ranks are repeated, since rows are updated in every iteration, with only the surviving_candidates being updated): " << std::flush;
            PrintMatrix(f_value_ranks, nnets, n_evals_each_it);

            for (int i = 0; i < nnets; i++)
            {
                cout << "|" << " i = " << i << " " << Average(f_values[i], n_evals_each_it) << endl; 
            }
            

            cout << endl;
            cout << "tmp_order: " << std::flush;
            PrintArray(tmp_order, nnets);

            cout << endl;

            double avg_perf_best = 0;


            cout << "BEST_FITNESS_DEBUG_LAST_IT, (reeval200, selection_best) ->" << 0 << ", " << Average(f_values[argmax(tmp_order, (int)nnets)], EVALS_TO_SELECT_BEST_CONTROLLER_IN_LAST_IT) << endl;
            
            cout << endl;
        }




        // save scaled fitness
        for (size_t inet = 0; inet < nnets; inet++)
        {
            NEAT::OrganismEvaluation eval;
            results[inet] = eval;
            results[inet].fitness = tmp_order[inet];
            results[inet].error = 2 - tmp_order[inet];
        }

        for (size_t i = 0; i < nnets+1; i++)
        {
            delete[] f_values[i];
            delete[] f_value_ranks[i];
        }
        delete[] f_values;
        delete[] f_value_ranks;

        delete[] tmp_order;
    }
};

} //namespace PERMU_MULTI


namespace NEAT
{

Permu_multiEvaluator::Permu_multiEvaluator()
{
    executor_multi = NEAT::NetworkExecutor<PERMU_MULTI::Evaluator>::create();
    parameters_multi = new PERMU_MULTI::params_multi();
    delete parameters;
    parameters = parameters_multi;
}

Permu_multiEvaluator::~Permu_multiEvaluator()
{
    delete executor_multi;
}

void Permu_multiEvaluator::read_conf_file(std::string conf_file_path)
{
    PermuEvaluator::read_conf_file(conf_file_path); // call parent read_config_file
    if(parameters->MODE != "train")
    {
        cerr << "Permu multiobjective with sveral instances is only available in train mode.\n"
        << "The trained controllers need to be used with the standard permu test mode." << endl;
        exit(1);
    }

    INIReader reader(conf_file_path);
    if (reader.ParseError() != 0)
    {
        std::cout << "Can't load " << conf_file_path << "\n";
        exit(1);
    }



    std::string COMMA_SEPARATED_LIST_OF_INSTANCE_PATHS = reader.Get("Global", "COMMA_SEPARATED_LIST_OF_INSTANCE_PATHS", "UNKNOWN");
    parameters_multi->VECTOR_OF_INSTANCE_PATHS = new std::vector<std::string>(split_string(COMMA_SEPARATED_LIST_OF_INSTANCE_PATHS, ","));
 
}

void Permu_multiEvaluator::execute(class NEAT::Network **nets_, class NEAT::OrganismEvaluation *results, size_t nnets)
{
    using namespace NEAT;
    env->pop_size = this->neat_params->POPSIZE_NEAT;
    PERMU_MULTI::Evaluator *ev = new PERMU_MULTI::Evaluator();
    this->parameters->neat_params = this->neat_params;
    ev->parameters = this->parameters_multi;
    ev->iteration_number = this->iteration_number;
    ev->best_network = this->best_network;
    ev->execute(nets_, results, nnets);

    this->best_network = ev->best_network;
    this->iteration_number++;
    delete ev;
}

void Permu_multiEvaluator::run_given_conf_file(std::string conf_file_path)
{
    using namespace std;
    using namespace NEAT;

    read_conf_file(conf_file_path);
    parameters->neat_params = this->neat_params;

    if (parameters->MODE == "train")
    {

        Experiment *exp = Experiment::get(parameters->prob_name.c_str());
        rng_t rng{parameters->SEED};
        exp->neat_params->global_timer.tic();
        exp->run(rng);
        return;
    }
    else
    {
        cerr << "invalid mode provided. Permu multi can only be used to train. Testing is shared with permu." << endl;
        exit(1);
    }
};




class NetworkEvaluator *create_permu_multi_evaluator()
{
    return new Permu_multiEvaluator();
}

} // namespace NEAT
