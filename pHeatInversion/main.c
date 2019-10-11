/* Straightforward case: simples heat equation on [0,1]
 * with zero boundary conditions. u = u(x, t)
 *     -d^2/dx u = du/dt on [0,1] for every time t>0
 *       u(x, 0) = u_D   on [0,1]
 *       u(0, t) = u(1, t) = 0 at every time t
 *
 * The G operator described in the README file is here so defined:
 *       - express u_0, starting condition (not necessarely known), 
 *         as basis expansion
 *         by using Fourier. Say that we stop at n = 3;
 *         It's our domain dimension;
 *       - by using the approximated u_0, solve the PDE and register
 *         the results at a fixed time, say 0.01
 *       - set, as output y, various space values of u_sol,
 *         say at x=0, 0.1, ..., x=1 (again, time has been fixed).
 *
 * Summing up we have the map:
 * G: R^domain_dim -> R^number_of_spatial_observations_at_time_0.01
 * 
 * and our aim will be to reverse it:
 * reconstruct an approximative initial condition by observing the y datas. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include "basics.h"
#include "ranvar.h"
#include "pcninv.h"
#include "fileio.h"
#include "heat_eq_g.c"

/* G is supposed to be included in some way */

/* Produce toy-model data. More precisely, it is assumed to
 * have G from R^domain_dim to R^codomain_dim.
 * The array x is initialized with random data, uniformly
 * between -10 and 10 (arbitrarely choice, no meaning).
 * Then y is created by applying G on x.
 * Finally, the values on y are noised.
 * Consequently, the aim of the main script will be to re-compute x
 * by having only y. Since the true values of x are known,
 * in this case will also be possible to compute the true error. 
 * Parameters:
 - noise: covariance of gaussian's noise;
 - x: array of dimension domain_dim, elements set randomly;
 - y: array of dimension codomain_dim; elements set as observations. */
void createToyData(double noise, double *x, int domain_dim,
                        double *y, int codomain_dim)
{
        int i = 0;

        /* Randomize the parameters x */
        for (i = 0; i < domain_dim; ++i){
                x[i] = rndmUniformIn(-10, 10);
        }

        /* Apply G to x */
        G((const double *) x, domain_dim, y, codomain_dim);
        printf("\n** noise-free obs: \n");
        printVec(y, codomain_dim);
        printf("\n");

        /* Put a noise on each value of y */
        for (i = 0; i < codomain_dim; ++i){
                y[i] += rndmGaussian(0, noise);
        }
}       
        

int main(int argc, char *argv[]){
        srand(time(NULL));

        /* Noise used to produce the toy models data;
         * Noise introduced in the MCMC algorithm */
        double data_noise = 1e-1; 
        double mcmc_noise = 1e-1;
        
        /* The algorithm is very sensitive to the number of
         * produced samples, and how many monte carlo cycles
         * are used to produce each of it.
         * Default values: 2^10, 2^12 (powers set later) */
        int n2 = 10;
        int mcmc2 = 12;

        /* Default value for the domain of G
         * and its codomain */
        int domain_dim = 2;
        int num_observations = 11;

        /* The values above can be modified via command arguments */
        if (argc >= 3){
                n2 = atoi(argv[1]);
                mcmc2 = atoi(argv[2]);
                if (argc == 5){
                /* Then also domain_dim and num_observations */
                        domain_dim = atoi(argv[3]);
                        num_observations = atoi(argv[4]);
                }
        }

                
        double *true_params = malloc(sizeof(double) * domain_dim);
        double *observed = malloc(sizeof(double) * num_observations);
        assert(true_params != NULL && observed != NULL);

        createToyData(data_noise, true_params, domain_dim,
                        observed, num_observations);

        printf("** true coeff: \n");
        printVec(true_params, domain_dim);
        printf("\n** noised obs: \n");
        printVec(observed, num_observations);   
        printf("\n");

        /* Now that the data are ready, set the bayes parameters */

        /* Output file where to write the posterior distribution */
        FILE *pfile = fopen("posterior_measure.txt", "w");
        assert(pfile != NULL);

        int n = (int) pow(2, n2);
        int mcmc = (int) pow(2, mcmc2);

        /* Residual error produced by the bayesian inversion */
        double err = 0;
        int i, j;
        
        /* Estimated parameters */
        double *map = malloc(sizeof(double) * domain_dim);
        /* Covariance matrix for the gaussian */
        double *cov = malloc(sizeof(double) * domain_dim * domain_dim);
        /* Starting point where to start the chain */
        double *start = malloc(sizeof(double) * domain_dim);
        assert(map != NULL && cov != NULL && start != NULL);

        /* Reset map, set a random starting point, a small covariance matrix */
        for (i = 0; i < domain_dim; ++i){
                map[i] = 0;
                start[i] = rndmUniformIn(-10., 10.);
                for (j = 0; j < domain_dim; ++j){
                        cov[i + j * domain_dim] = (i == j) ? 0.9 : 0.1;
                }
        }

        printf("** Starting point:\n");
        printVec(start, domain_dim);
        printf("\n%d samples, %d iterations per sample\n", n, mcmc);

        /* Proceed with the bayesian inversion:
         * n = number of posterior samples to produces;
         * mcmc = number of monte carlo iterations;
         * map = most frequent sample = solution = MAP
         * true_params = true known parameters (toy model data)
         * G = the linear interpolation defined above
         * observed = vector containing the y_i
         * domain_dim = domain's dimension
         * observed = codomain's dimension
         * noise during the mcmc chain = mcmc_noise
         * 0.2 = beta, parameter for the pCN algorithm 0<beta<1
         * cov = my covariance matrix, prior gaussian
         * start = starting point for the chain
         * pfile = file to write posterior distribution (values, probabilities)
         * 0 = no verbose/debug mode */
        err = bayInv(n, mcmc, map, true_params, G, observed,
                        domain_dim, num_observations,
                        mcmc_noise, 0.2, cov, start, pfile, 0);

        /* err contains the residual error, i.e. G(MAP) - observations */
        /* Print the results */
        printf("MAP: ");
        printVec(map, domain_dim);
        printf("RES ERR: %.3f%%\n", err);
        printf("Observed output:\n");
        printVec(observed, num_observations);
        printf("MAP output :\n");
        G(map, domain_dim, observed, num_observations);
        printVec(observed, num_observations);

        /* Free all the allocated memory */
        free(true_params);
        free(observed);
        free(map);
        free(cov);
        free(start);
        fclose(pfile);
        return 0;
}
