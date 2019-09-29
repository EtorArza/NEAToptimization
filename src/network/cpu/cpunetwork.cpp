/*
 Copyright 2001 The University of Texas at Austin

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef ENABLE_CUDA

#include "std.h" // Must be included first. Precompiled header with standard library includes.
#include "cpunetwork.h"
#include "neat.h"
#include "util.h"
#include <assert.h>
#include "networkexecutor.h"

using namespace NEAT;
using namespace std;


Network *Network::create() {
    return new CpuNetwork();
}

// copy constructor
CpuNetwork::CpuNetwork(const CpuNetwork&  other){ 
    this->activations = std::vector<real_t>(other.activations);

    this->dims = other.dims;
    this->links = other.links;
    this->nodes = other.nodes;

}




// Requires nodes to be sorted by type: BIAS, SENSOR, OUTPUT, HIDDEN
void CpuNetwork::configure(const NetDims &dims_,
                           NetNode *nodes_,
                           NetLink *links_) {
    this->dims = dims_;

    nodes.resize(dims.nnodes.all);
    for(size_t i = 0; i < dims.nnodes.all; i++) {
        nodes[i] = nodes_[i];
    }

    links.resize(dims.nlinks);
    for(size_t i = 0; i < dims.nlinks; i++) {
        links[i] = links_[i];
    }

    activations.resize(dims.nnodes.all);
    for(size_t i = 0; i < dims.nnodes.bias; i++) {
        activations[i] = 1.0;
    }
    for(size_t i = dims.nnodes.bias; i < dims.nnodes.all; i++) {
        activations[i] = 0.0;
    }
}

void CpuNetwork::clear_noninput() {
    std::fill(activations.begin()+dims.nnodes.input, activations.end(), 0);
}

void CpuNetwork::load_sensor(size_t isensor,
                             real_t activation) {
    activations[dims.nnodes.bias + isensor] = activation;
}




real_t *CpuNetwork::get_outputs() {
    return activations.data() + dims.nnodes.input;
}

void CpuNetwork::activate() {
    real_t act_other[dims.nnodes.all];

    //Copy only input activation state.
    memcpy(act_other,
           activations.data(),
           sizeof(real_t) * dims.nnodes.input);

    real_t *act_curr = activations.data(), *act_new = act_other;
    //cout << "--" << endl;
    for(size_t icycle = 0; icycle < NACTIVATES_PER_INPUT; icycle++) {

        for(size_t i = dims.nnodes.input; i < dims.nnodes.all; i++) {
            NetNode &node = nodes[i];

            real_t sum = 0.0;
            for(size_t j = node.incoming_start; j < node.incoming_end; j++) {
                NetLink &link = links[j];
                sum += link.weight * act_curr[link.in_node_index];
                //cout << "from=" << (link.in_node_index) << ", to=" << (i) << ", weight=" << link.weight << ", act[from]=" << act_curr[link.in_node_index] << ", partial=" << link.weight * act_curr[link.in_node_index] << ", sum=" << sum << endl;
            }

            // act_new[i] = NEAT::fsigmoid(sum, 4.924273, 2.4621365);  // Sigmoidal activation.
            act_new[i] = NEAT::ftanh(sum);  // tanh activation function.
        }

        std::swap(act_curr, act_new);
    }

    if(act_curr != activations.data()) {
        // If an odd number of cycles, we have to copy non-input data
        // of act_other back into activations.
        memcpy(activations.data() + dims.nnodes.input,
               act_other + dims.nnodes.input,
               sizeof(real_t) * (dims.nnodes.all - dims.nnodes.input));
    }
}

vector<real_t> &CpuNetwork::get_activations(__out vector<real_t> &result) {
    return result = activations;
}

void CpuNetwork::set_activations(__in vector<real_t> &newacts) {
    activations = newacts;
}

#endif // ENABLE_CUDA
