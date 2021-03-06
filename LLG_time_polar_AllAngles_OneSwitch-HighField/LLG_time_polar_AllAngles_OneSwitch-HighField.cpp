#include "stdafx.h"
#include <stdio.h>
#include <math.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <numeric>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>

using namespace std;

#define npart 1
#define neq (2*npart)

#define nstep 500

const int n_max_vec = 30;

const double Pi = 3.1415926535897932384626433832795;
const double uround = 1.0e-8;

struct sAnizo
{
public: double ax, ay, az;
};
struct sReadData
{
public: double x, y, z, theta_ea, phi_ea, volume, k, Msat, theta_sol, phi_sol;
};
struct sCoef
{
public: int vecin;
		double C, coef, xx, xy, xz, yx, yy, yz, zx, zy, zz;
};
struct  Camp
{
public: double H, theta, phi, Hx, Hy, Hz;
};
struct Moment
{
public: double M, theta, phi, Mx, My, Mz;
};

void position_coeficients(struct sReadData D1, struct sReadData D2, struct sCoef *Pos_Coef, double *dist);
void function_neighbours(void);
void anisotropy_coef(void);
int fcn(double t, const double y[], double yprime[], void *params);
//void fcn(int n_equ, double t, double y[], double yprime[]);
int fcn_xyz(double t, const double y[], double yprime[], void *params);
//void fcn_xyz(int n_equ, double t, double y[], double yprime[]);
double stability_test(double solutie[], double solutie_old[]);

static struct sAnizo A[npart];
struct sReadData Medium[npart];
static struct Camp H[nstep], Hext;
static struct Moment M[nstep][npart], Msys;
static double Hx_part[npart];
static double Hy_part[npart];
static double Hz_part[npart];

static int neighbours[npart];
static struct sCoef Position_Coef[npart][n_max_vec];

//******************************************************

const double VolumTotal = 1 * npart;

const double alpha = 0.01;
const double miu0 = 4.0e-7*Pi;

const double Ms = 795774.7154594767; // 2.668e5;
const double K1 = 1.0e5;

const double gamma = 2.210173e5;
const double time_norm = (1.0 + alpha * alpha) / gamma / Ms;

double theta_ea = 0.1 * Pi / 180.0;
double phi_ea = 0.1 * Pi / 180.0;

double theta_h = 10.0 * Pi / 180.0;
double phi_h = 0.1 * Pi / 180.0;

double theta_0 = theta_ea + 1.0e-4;
double phi_0 = phi_ea + 1.0e-4;

const double Hk = 2.0*fabs(K1) / miu0 / Ms / Ms;

double T_Larmor = 1.0 / (gamma * 2.0*fabs(K1) / (miu0*Ms) / (2.0*Pi))*1.0e12;

const double Exch = 0.1;
const double prag_vecini = 5.0e-3;
const double FieldMax = +1193662.0 / Ms;
const double FieldMin = -1193662.0 / Ms;

//******************************************************

int main()
{
	int i, j;

	double       t = 0.0;
	double       tend = 2000.0;
	double		 tstep = 1.0 / (time_norm*1.0e12);
	double		last_t = 0.0;
	double       y[neq], y_old[neq];

	int status;

	double		torq_dif;

	for (i = 0; i < nstep / 2; i++)
	{
		H[i].H = FieldMax - (FieldMax - FieldMin) * i / (nstep / 2 - 1);
		H[i].theta = theta_h;
		H[i].phi = phi_h;
		H[i].Hx = H[i].H * sin(H[i].theta) * cos(H[i].phi);
		H[i].Hy = H[i].H * sin(H[i].theta) * sin(H[i].phi);
		H[i].Hz = H[i].H * cos(H[i].theta);
	}
	for (i = nstep / 2; i < nstep; i++)
	{
		H[i].H = FieldMin + (FieldMax - FieldMin) * (i - nstep / 2) / (nstep / 2 - 1);
		H[i].theta = theta_h;
		H[i].phi = phi_h;
		H[i].Hx = H[i].H * sin(H[i].theta) * cos(H[i].phi);
		H[i].Hy = H[i].H * sin(H[i].theta) * sin(H[i].phi);
		H[i].Hz = H[i].H * cos(H[i].theta);
	}

	for (i = 0; i < npart; i++)
	{
		Medium[i].x = 0.0;
		Medium[i].y = 0.0;
		Medium[i].z = 0.0;
		Medium[i].volume = 1.0;
		Medium[i].Msat = Ms;
		Medium[i].k = 1.0;
		Medium[i].theta_ea = theta_ea;
		Medium[i].phi_ea = phi_ea;
		Medium[i].theta_sol = theta_0;
		Medium[i].phi_sol = phi_0;
	}

	for (i = 0; i < npart; i++)
	{
		y[2 * i + 0] = theta_0;
		y[2 * i + 1] = phi_0;
	}

	anisotropy_coef();
	function_neighbours();

	//////////////////////////////////////////////////////////////////////////
	// MHL
	//////////////////////////////////////////////////////////////////////////
	//int LIMIT_STEPS = 19;
	char save_file[500] = "D:\\Stoleriu\\C\\special\\3d\\res\\2018\\SW---LLG\\_ONE_SWITCH_\\MHL\\LLG_time_Js1-K1e5_th10_5Hk_ONE-SWITCH.dat";

	FILE *fp;
	int num_LLG = 0;

	fp = fopen(save_file, "w");
	fclose(fp);

	char *state;

	gsl_odeiv2_system sys = { fcn, NULL, neq, NULL };

	double ug_th = theta_h;
	double ug_ph = phi_h;
	int ii;
	double MHL_projection, cos_ug_cu_ea, Energy;
	vector<double> timpi;
	vector<double> valori;


	//initial conditions
	for (i = 0; i < npart; i++)
	{
		y[2 * i + 0] = theta_h;
		y[2 * i + 1] = phi_h;
	}

	t = 0.0;
	last_t = 0.0;

	//for (theta_0 = 0.1 * Pi / 180; theta_0 < 179.1 * Pi / 180; theta_0 += 1.0 * Pi / 180.0)
	//{
	//	for (ug_th = 179.1 * Pi / 180; ug_th >= -179.1 * Pi / 180; ug_th -= 1.0 * Pi / 180.0)
	//	{
	// 	  for (ug_ph = 0.0 * Pi / 180; ug_ph < 180.1 * Pi / 180; ug_ph += 180.0 * Pi / 180.0)
	// 	  {
	//for (ii = 0; ii < nstep; ii++)
	{
		//for (ug_th = -179.9*Pi / 180; ug_th < 179.9*Pi / 180; ug_th += 5.0*Pi / 180)
		//{
		ii = nstep / 2;

		H[ii].H = -5.0 * Hk;

		H[ii].theta = theta_h;
		H[ii].phi = phi_h;
		H[ii].Hx = H[ii].H * sin(H[ii].theta) * cos(H[ii].phi);
		H[ii].Hy = H[ii].H * sin(H[ii].theta) * sin(H[ii].phi);
		H[ii].Hz = H[ii].H * cos(H[ii].theta);


		for (j = 0; j < neq; j++)
			y_old[j] = y[j];


		Hext = H[ii];
		num_LLG = 0;


		gsl_odeiv2_driver *d = gsl_odeiv2_driver_alloc_y_new(&sys, gsl_odeiv2_step_rk8pd, 1e-6, 1e-8, 1e-8);

		fp = fopen(save_file, "a");

		while (((torq_dif = stability_test(y, y_old)) > 1.0e-4) || (num_LLG < 100))
			//for(int numar_steps=0; numar_steps<LIMIT_STEPS; numar_steps++)
			//while( ((t*time_norm*1.0e12) - last_t)<30.0 )
		{
			gsl_odeiv2_driver_apply(d, &t, t + tstep, y);

			Msys.Mx = 0.0; Msys.My = 0.0; Msys.Mz = 0.0;

			for (j = 0; j < npart; j++)
			{
				Msys.Mx += Medium[j].volume * sin(y[2 * j + 0])*cos(y[2 * j + 1]);
				Msys.My += Medium[j].volume * sin(y[2 * j + 0])*sin(y[2 * j + 1]);
				Msys.Mz += Medium[j].volume * cos(y[2 * j + 0]);
			}

			num_LLG++;

			//if (!(num_LLG % 100))
			//	printf("torq-diff ->>  %le\n", torq_dif);

			MHL_projection = (Msys.Mx*sin(H[ii].theta)*cos(H[ii].phi) + Msys.My*sin(H[ii].theta)*sin(H[ii].phi) + Msys.Mz*cos(H[ii].theta));
			// 
			// 			cos_ug_cu_ea = (Msys.Mx*A[0].ax + Msys.My*A[0].ay + Msys.Mz* A[0].az);
			// 			Energy = -miu0 * Ms * (Hk * cos_ug_cu_ea * cos_ug_cu_ea + H[ii].H * MHL_projection) / 2.0;

			//fprintf(fp, "%20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf\n", t*time_norm*1.0e12, H[ii].Hx, H[ii].Hy, H[ii].Hz, Msys.Mx, Msys.My, Msys.Mz, H[ii].H, MHL_projection);

			fprintf(fp, "%20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf %20.16lf\n",
				t*time_norm*1.0e12, 
				t*time_norm*1.0e12 / (T_Larmor * Hk / fabs(H[ii].H)), 
				(T_Larmor * Hk / fabs(H[ii].H)), 
				(2.0*Pi) * 1.0e12 / (gamma * fabs(H[ii].H) * Ms), 
				Msys.Mx, Msys.My, Msys.Mz, H[ii].H, MHL_projection);
		}

		last_t = (t*time_norm*1.0e12);

		fclose(fp);
		gsl_odeiv2_driver_free(d);

		// 		fp = fopen(save_file, "a");
		//
		// 		double ug_target = acos(Msys.Mx*sin(theta_ea)*cos(phi_ea) + Msys.My*sin(theta_ea)*sin(phi_ea) + Msys.Mz*cos(theta_ea));
		// 		if (Msys.Mx < 0.0)
		// 		{
		// 			ug_target *= -1.0;
		// 		}
		//
		// 		fprintf(fp, "%20.16lf %20.16lf %20.16lf\n", (theta_0 - theta_ea), ug_target, t*time_norm*1.0e12 / T_Larmor);
		// 		fclose(fp);

		//}
		printf("%07.4lf -> %07.4lf \n", H[ii].H, MHL_projection);
	}

	return(0);
}

//**************************************************************************
//**************************************************************************
//**************************************************************************
//**************************************************************************
//**************************************************************************

void position_coeficients(struct sReadData D1, struct sReadData D2, sCoef *Pos_Coef, double *dist)
{
	*dist = sqrt((D2.x - D1.x)*(D2.x - D1.x) + (D2.y - D1.y)*(D2.y - D1.y) + (D2.z - D1.z)*(D2.z - D1.z));
	double r = (double)rand() / RAND_MAX;

	if (*dist == 0)
	{
		Pos_Coef->C = r * 0.1;

		Pos_Coef->coef = 0.0;
		Pos_Coef->xx = 0.0;
		Pos_Coef->xy = 0.0;
		Pos_Coef->xz = 0.0;
		Pos_Coef->yx = 0.0;
		Pos_Coef->yy = 0.0;
		Pos_Coef->yz = 0.0;
		Pos_Coef->zx = 0.0;
		Pos_Coef->zy = 0.0;
		Pos_Coef->zz = 0.0;
	}
	else
	{
		double rx = (D2.x - D1.x) / *dist;
		double ry = (D2.y - D1.y) / *dist;
		double rz = (D2.z - D1.z) / *dist;

		Pos_Coef->C = 0.0;

		Pos_Coef->coef = D2.volume * D2.Msat / 4.0 / Pi / *dist / *dist / *dist / Ms;

		Pos_Coef->xx = 3.0 * rx * rx - 1.0;
		Pos_Coef->xy = 3.0 * rx * ry;
		Pos_Coef->xz = 3.0 * rx * rz;
		Pos_Coef->yx = Pos_Coef->xy;
		Pos_Coef->yy = 3.0 * ry * ry - 1.0;
		Pos_Coef->yz = 3.0 * ry * rz;
		Pos_Coef->zx = Pos_Coef->xz;
		Pos_Coef->zy = Pos_Coef->yz;
		Pos_Coef->zz = 3.0 * rz * rz - 1.0;
	}
}

//**************************************************************************

void function_neighbours(void)
{
	sReadData Data1, Data2;
	sCoef Pos_Coef;
	double distance;
	int neighbours_max = 0, neighbours_med = 0;
	int f;

	for (int i = 0; i < npart; i++)
	{
		neighbours[i] = 0;
		Data1 = Medium[i];

		for (f = 0; f < npart; f++)
		{
			Data2 = Medium[f];

			if ((i != f))
			{
				position_coeficients(Data1, Data2, &Pos_Coef, &distance);
				if (distance < 1.0e-9)
				{
					if (neighbours[i] + 1 > n_max_vec - 1)
					{
						printf("\n\n PREA MULTI VECINI !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n\n");
						getchar();
						break;
					}
					else
					{
						neighbours[i]++;
						Pos_Coef.vecin = f;  //<<<---- asta e vecin
						Position_Coef[i][neighbours[i] - 1] = Pos_Coef;
					}
					continue; // din acelasi cub
				}

				if (Pos_Coef.coef > prag_vecini)
				{
					if (neighbours[i] + 1 > n_max_vec - 1)
					{
						printf("\n\n PREA MULTI VECINI !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! \n\n");
						getchar();
						break;
					}
					else
					{
						neighbours[i]++;
						Pos_Coef.vecin = f;  //<<<---- asta e vecin
						Position_Coef[i][neighbours[i] - 1] = Pos_Coef;
					}
				}
			}
		}

		printf("particula: %d  cu  %d  vecini \n", i, neighbours[i]);
		neighbours_med += neighbours[i];
		if (neighbours[i] > neighbours_max) neighbours_max = neighbours[i];
	}
	printf("Numar maxim de vecini: %d      Numar mediu de vecini: %f \n", neighbours_max, (float)neighbours_med / npart);
}

//**************************************************************************

void anisotropy_coef(void)
{
	for (int i = 0; i < npart; i++)
	{
		A[i].ax = sin(Medium[i].theta_ea)*cos(Medium[i].phi_ea);
		A[i].ay = sin(Medium[i].theta_ea)*sin(Medium[i].phi_ea);
		A[i].az = cos(Medium[i].theta_ea);
	}
}

//**************************************************************************

int fcn(double time, const double input[], double deriv[], void *params)
//void fcn(int n_equ, double t, double input[], double deriv[])
{
	int i, j, k, kk;
	double s0, c0, s1, c1;
	double term1, term2, term3, factor;
	double mp_x, mp_y, mp_z;

	for (i = 0; i < npart; i++)                 //  Fcn_Dopri(x,y,k1), sol->y era param in Dopri
	{
		k = 2 * i;

		s0 = sin(input[k]);
		c0 = cos(input[k]);
		s1 = sin(input[k + 1]);
		c1 = cos(input[k + 1]);

		term1 = A[i].ax*s0*c1 + A[i].ay*s0*s1 + A[i].az*c0;
		term2 = -A[i].ax*s0*s1 + A[i].ay*s0*c1;
		term3 = A[i].ax*c0*c1 + A[i].ay*c0*s1 - A[i].az*s0;
		factor = 1.0 /*+ 2.0*K2*term1*term1 / K1 + 3.0*K3*term1*term1*term1*term1 / K1*/;

		Hx_part[i] = Hext.Hx + Medium[i].k * Hk * factor * (-term1 * term2*s1 / s0 + term1 * term3*c0*c1);
		Hy_part[i] = Hext.Hy + Medium[i].k * Hk * factor * (term1*term2*c1 / s0 + term1 * term3*c0*s1);
		Hz_part[i] = Hext.Hz + Medium[i].k * Hk * factor * (-term1 * term3*s0);

		for (j = 0; j < neighbours[i]; j++)
		{
			kk = 2 * Position_Coef[i][j].vecin;
			mp_x = sin(input[kk])*cos(input[kk + 1]);
			mp_y = sin(input[kk])*sin(input[kk + 1]);
			mp_z = cos(input[kk]);

			Hx_part[i] += Position_Coef[i][j].C*Hk*mp_x + Position_Coef[i][j].coef * (Position_Coef[i][j].xx*mp_x + Position_Coef[i][j].xy*mp_y + Position_Coef[i][j].xz*mp_z);
			Hx_part[i] += Position_Coef[i][j].C*Hk*mp_y + Position_Coef[i][j].coef * (Position_Coef[i][j].yx*mp_x + Position_Coef[i][j].yy*mp_y + Position_Coef[i][j].yz*mp_z);
			Hx_part[i] += Position_Coef[i][j].C*Hk*mp_z + Position_Coef[i][j].coef * (Position_Coef[i][j].zx*mp_x + Position_Coef[i][j].zy*mp_y + Position_Coef[i][j].zz*mp_z);
		}

		deriv[k] = Hx_part[i] * (alpha * c0 * c1 - s1) + Hy_part[i] * (alpha * c0 * s1 + c1) - Hz_part[i] * alpha * s0;
		deriv[k + 1] = (-Hx_part[i] * (alpha * s1 + c0 * c1) + Hy_part[i] * (alpha * c1 - c0 * s1) + Hz_part[i] * s0) / s0;
	}

	return GSL_SUCCESS;
}

//**************************************************************************

int fcn_xyz(double time, const double input[], double deriv[], void *params)
//void fcn_xyz(int n_equ, double t, double input[], double deriv[])
{
	int i, j, k, kk;
	double s0, c0, s1, c1;
	double term1, term2, term3;
	double aux;
	double Hkx, Hky, Hkz;
	struct Camp Htot;
	double mp_x, mp_y, mp_z;

	for (i = 0; i < npart; i++)                 //  Fcn_Dopri(x,y,k1), sol->y era param in Dopri
	{
		k = 3 * i;

		aux = A[i].ax*input[k + 0] + A[i].ay*input[k + 1] + A[i].az*input[k + 2];

		Hx_part[i] = Hext.Hx - Medium[i].k * Hk * aux * A[i].ax;
		Hy_part[i] = Hext.Hy - Medium[i].k * Hk * aux * A[i].ax;
		Hz_part[i] = Hext.Hz - Medium[i].k * Hk * aux * A[i].ax;

		for (j = 0; j < neighbours[i]; j++)
		{
			kk = 3 * Position_Coef[i][j].vecin;
			mp_x = input[kk + 0];
			mp_y = input[kk + 1];
			mp_z = input[kk + 2];

			Hx_part[i] += Position_Coef[i][j].C*Hk*mp_x + Position_Coef[i][j].coef * (Position_Coef[i][j].xx*mp_x + Position_Coef[i][j].xy*mp_y + Position_Coef[i][j].xz*mp_z);
			Hy_part[i] += Position_Coef[i][j].C*Hk*mp_y + Position_Coef[i][j].coef * (Position_Coef[i][j].yx*mp_x + Position_Coef[i][j].yy*mp_y + Position_Coef[i][j].yz*mp_z);
			Hz_part[i] += Position_Coef[i][j].C*Hk*mp_z + Position_Coef[i][j].coef * (Position_Coef[i][j].zx*mp_x + Position_Coef[i][j].zy*mp_y + Position_Coef[i][j].zz*mp_z);
		}
		//(Hy1*mz(t)                - Hz1*my(t))              - alpha1*(Hy1*mx(t)*my(t)                         + Hz1*mx(t)*mz(t)                        - Hx1*my(t)*my(t)                          - Hx1*mz(t)*mz(t))
		deriv[k + 0] = (Hy_part[i] * input[k + 2] - Hz_part[i] * input[k + 1]) - alpha * (Hy_part[i] * input[k + 0] * input[k + 1] + Hz_part[i] * input[k + 0] * input[k + 2] - Hx_part[i] * input[k + 1] * input[k + 1] - Hx_part[i] * input[k + 2] * input[k + 2]);

		//(Hz1*mx(t)             - Hx1*mz(t))                 - alpha1*(Hx1*mx(t)*my(t)                         + Hz1*my(t)*mz(t)                        - Hy1*mx(t)*mx(t)                          - Hy1*mz(t)*mz(t))
		deriv[k + 1] = (Hz_part[i] * input[k + 0] - Hx_part[i] * input[k + 2]) - alpha * (Hx_part[i] * input[k + 0] * input[k + 1] + Hz_part[i] * input[k + 1] * input[k + 2] - Hy_part[i] * input[k + 0] * input[k + 0] - Hy_part[i] * input[k + 2] * input[k + 2]);

		//(Hx1*my(t)              - Hy1*mx(t))                - alpha1*(Hx1*mx(t)*mz(t)                         + Hy1*my(t)*mz(t)                       - Hz1*mx(t)*mx(t)                           - Hz1*my(t)*my(t))
		deriv[k + 2] = (Hx_part[i] * input[k + 1] - Hy_part[i] * input[k + 0]) - alpha * (Hx_part[i] * input[k + 0] * input[k + 2] + Hy_part[i] * input[k + 1] * input[k + 2] - Hz_part[i] * input[k + 0] * input[k + 0] - Hz_part[i] * input[k + 1] * input[k + 1]);
	}

	return GSL_SUCCESS;
}

//**************************************************************************

double stability_test(double solutie[], double solutie_old[])
{
	double diference = 0.0;
	double torq, torq_old;
	double proj, proj_old;
	double th, ph, th_old, ph_old;

	for (int i = 0; i < npart; i++)
	{
		if (fabs(solutie_old[2 * i + 0] - solutie[2 * i + 0]) > diference)
			diference = fabs(solutie_old[2 * i + 0] - solutie[2 * i + 0]);

		if (fabs(solutie_old[2 * i + 1] - solutie[2 * i + 1]) > diference)
			diference = fabs(solutie_old[2 * i + 1] - solutie[2 * i + 1]);

		//solutie_old[2 * i + 0] = solutie[2 * i + 0];
		//solutie_old[2 * i + 1] = solutie[2 * i + 1];

		th = atan(tan(solutie[2 * i + 0]));
		ph = atan(tan(solutie[2 * i + 1]));

		torq = (th * Hext.phi - ph * Hext.theta) * (th * Hext.phi - ph * Hext.theta) + (ph - Hext.phi) * (ph - Hext.phi) + (Hext.theta - th) * (Hext.theta - th);
		proj = (sin(th)*cos(ph)*sin(Hext.theta)*cos(Hext.phi) + sin(th)*sin(ph)*sin(Hext.theta)*sin(Hext.phi) + cos(th)*cos(Hext.theta));

		th_old = atan(tan(solutie_old[2 * i + 0]));
		ph_old = atan(tan(solutie_old[2 * i + 1]));

		//solutie_old[2 * i + 0] = solutie[2 * i + 0];
		//solutie_old[2 * i + 1] = solutie[2 * i + 1];

		torq_old = (th_old * Hext.phi - ph_old * Hext.theta) * (th_old * Hext.phi - ph_old * Hext.theta) + (ph_old - Hext.phi) * (ph_old - Hext.phi) + (Hext.theta - th_old) * (Hext.theta - th_old);
		proj_old = (sin(th_old)*cos(ph_old)*sin(Hext.theta)*cos(Hext.phi) + sin(th_old)*sin(ph_old)*sin(Hext.theta)*sin(Hext.phi) + cos(th_old)*cos(Hext.theta));

		if (fabs(torq_old - torq) > diference)
			diference = fabs(torq_old - torq);

		solutie_old[2 * i + 0] = solutie[2 * i + 0];
		solutie_old[2 * i + 1] = solutie[2 * i + 1];

		//  		if (fabs(proj_old - proj) > diference)
		//  			diference = fabs(proj_old - proj);
	}

	return diference;
}

//**************************************************************************