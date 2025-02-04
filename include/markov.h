/*---------------------------------------------------------------
 * Copyright (c) 2024
 * Broadcom Corporation
 * All Rights Reserved.
 *---------------------------------------------------------------
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and
 * the following disclaimers.
 *
 *
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimers in the documentation and/or other materials
 * provided with the distribution.
 *
 *
 * Neither the name of Broadcom Coporation,
 * nor the names of its contributors may be used to endorse
 * or promote products derived from this Software without
 * specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE CONTIBUTORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ________________________________________________________________
 *
 * markov
 * Suppport for markov graph
 *
 * by Robert J. McMahon (rjmcmahon@rjmcmahon.com, bob.mcmahon@broadcom.com)
 * -------------------------------------------------------------------
 */
#ifndef MARKOVC_H
#define MARKOVC_H
#ifdef __cplusplus
extern "C" {
#endif

#define FLOATTOLERANCE 0.00001
#define FloatEqualZero(val) (fabs(val) < FLOATTOLERANCE)
#define FloatLessThanZero(val) (val < 0.0)
#define FloatEqualOne(val) (fabs(val - 1.0) < FLOATTOLERANCE))
#define FloatLessThanOne(val) ((1.0 - val) >  FLOATTOLERANCE)
#define FloatGreaterThanOne(val) ((val - 1.0) > FLOATTOLERANCE)

struct markov_entry {
    int node_len;
    int adjacent_node_len;
    uintmax_t transition_cnt;
    float prob_cummulative;
    float prob;
};

struct markov_graph {
    char *braket_str;
    int count;
    uintmax_t transition_cnt;
    uintmax_t unknown_cnt;
    int seed;
    int cur_row;
    int cur_col;
    bool node_known;
    struct markov_entry **entrys;
};

extern struct markov_graph *markov_graph_init (char *braket_option);
extern void markov_graph_free (struct markov_graph *graph);
extern int markov_graph_next (struct markov_graph *graph);
extern void markov_graph_set_seed (struct markov_graph *graph, int seed);
extern void markov_graph_print(struct markov_graph *graph, char *prepend);
extern bool markov_graph_count_edge_transition (struct markov_graph *graph, int node_len);

#ifdef __cplusplus
}
#endif
#endif // MARKOV_H
