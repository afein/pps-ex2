/***** Tiled LU decomposition*****/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "common.h"

void mm(double ** a1,int x_1,int y_1, double ** a2, int x_2, int y_2 , double ** res, int x_3, int y_3, double ** tmp_res, int M, int N, int P, int sign);
void mm_lower(double ** a1,int x_1,int y_1, double ** a2, int x_2, int y_2 , double ** res, int x_3, int y_3, int M, int N, int P);
void mm_upper(double ** a1,int x_1,int y_1, double ** a2, int x_2, int y_2 , double ** res, int x_3, int y_3, int M, int N, int P);
void rectrtri_lower(double ** a, int x_s,int y_s, int N, int M, double ** tmp_res);
void rectrtri_upper(double ** a, int x_s, int y_s, int N, int M, double ** tmp_res);
double ** allocate(int X, int Y);
int min(int x, int y)
{
    return x<y?x:y;
}
double ** get_inv_l(double ** a, int xs, int ys, int X, int Y);
double ** get_inv_u(double **a, int xs, int ys, int X, int Y);
void input(double ** a, int X, int Y);
void lu(double ** a, int num_blocks, int B);
void lu_kernel(double ** a, int xs, int ys, int X, int Y);
void print(double ** a, int N);
void mm_update(double ** a1, int x_1, int y_1, double ** a2, int x_2, int y_2, double ** res, int x_3, int y_3, int M, int N, int P);
double ** up_res, ** low_res;

int main(int argc, char *argv[])
{
    double ** A;
    int N,B,num_blocks;
    struct timeval ts, tf;
    double time;

    Matrix *mat;
    tiled_usage(argc, argv);

    mat = get_matrix(argv[1], 0, CONTINUOUS);
    N = mat->N;
    A = appoint_2D(mat->A, N, N);
    sscanf(argv[3],"%d",&B);

    if (N%B!=0 || B==1) {
        printf("\t Grid is not a multiple of Block size \n");
        exit(0);
    }

    num_blocks=N/B;

    low_res=allocate(B,(num_blocks-1)*B);
    up_res=allocate((num_blocks-1)*B,B);

    gettimeofday(&ts,NULL);

    lu(A,num_blocks,B);

    gettimeofday(&tf,NULL);
    time=(tf.tv_sec-ts.tv_sec)+(tf.tv_usec-ts.tv_usec)*0.000001;
    printf("Tiled\t%d\t%lf\tBlock Size: %d\n", N,time,B);

    upper_triangularize(N, A);
    print_matrix_2d_to_file(argv[2], N, N, *A);
    return 0;
}

/**** Tiled LU decomposition *****/
void lu(double **a, int num_blocks, int B)
{
    int i,j,k;
    double ** l_inv, ** u_inv;
    for (k=0; k<num_blocks-1; k++) {

        /****Compute LU decomposition on upper left tile*****/
        lu_kernel(a,k*B,k*B,B,B);

        /****Compute inverted L and U matrices of upper left tile*****/
        l_inv = cilk_spawn get_inv_l(a,k*B,k*B,B,B);
        u_inv = cilk_spawn get_inv_u(a,k*B,k*B,B,B);
        cilk_sync;


        /*****Compute LU decomposition on upper horizontal frame and left vertical frame*****/
        for_spawn_mm(k+1, num_blocks-1, k, num_blocks, a, l_inv, u_inv, B);

        /*****Update trailing blocks*****/
        for_spawn_updates(k+1, num_blocks-1, k, num_blocks, a, B);
        cilk_sync;
    }

    /***** Compute LU on final diagonal block *****/
    lu_kernel(a,(num_blocks-1)*B,(num_blocks-1)*B,B,B);

}

void for_spawn_mm(int i, int stop, int k, int num_blocks, double **a, double **l_inv, double **u_inv, int B)
{
    int current_range;

    current_range = stop - i;
    if(current_range > 0) {
        cilk_spawn for_spawn_mm(i, i + current_range / 2, k, num_blocks, a, l_inv, u_inv, B);
        cilk_spawn for_spawn_mm(i + current_range / 2 + 1, stop, k, num_blocks, a, l_inv, u_inv, B);
    }
    else if (current_range == 0) {
        cilk_spawn mm_lower(l_inv,0,0,a,k*B,i*B,a,k*B,i*B,B,B,B);
        cilk_spawn mm_upper(a,i*B,k*B,u_inv,0,0,a,i*B,k*B,B,B,B);
    }
}

void for_spawn_inner_updates(int start, int stop, int k, int num_blocks, int i, double **a, int B)
{
    int current_range;

    current_range = stop - start;
    if(current_range > 0) {
        cilk_spawn for_spawn_inner_updates(start, start + current_range / 2, k, num_blocks, i, a, B);
        cilk_spawn for_spawn_inner_updates(start + current_range / 2 + 1, stop, k, num_blocks, i, a, B);
    }
    else if (current_range == 0) {
        cilk_spawn mm_update(a,i*B,k*B,a,k*B,start*B,a,i*B,start*B,B,B,B);
    }
}

void for_spawn_updates(int i, int stop, int k, int num_blocks, double **a, int B)
{
    int current_range;

    current_range = stop - i;
    if(current_range > 0) {
        cilk_spawn for_spawn_updates(i, i + current_range / 2, k, num_blocks, a, B);
        cilk_spawn for_spawn_updates(i + current_range / 2 + 1, stop, k, num_blocks, a, B);
    }
    else if (current_range == 0) {
        cilk_spawn for_spawn_inner_updates(k+1, num_blocks-1, k, num_blocks, i, a, B);
    }
}




/***** Baseline LU Kernel *****/
void lu_kernel(double ** a, int xs,int ys, int X, int Y)
{
    int i,j,k;
    double l;
    for (k=0; k<min(X,Y); k++)
        for (i=k+1; i<X; i++) {
            l=a[i+xs][k+ys]=a[i+xs][k+ys]/a[k+xs][k+ys];
            for (j=k+1; j<Y; j++)
                a[i+xs][j+ys]-=l*a[k+xs][j+ys];
        }

}

/***** Computes the inverted L^(-1) matrix of upper diagonal block using rectrtri_lower() *****/
double ** get_inv_l(double ** a, int xs, int ys, int X, int Y)
{
    double ** l_inv=allocate(X,Y);
    double ** tmp_res=allocate(X,Y);
    int i,j;
    for (i=0; i<X; i++)
        for (j=0; j<Y; j++) {
            if (i>j)
                l_inv[i][j]=a[i+xs][j+ys];
            else if (i==j)
                l_inv[i][j]=1.0;
            else l_inv[i][j]=0;
        }
    rectrtri_lower(l_inv,0,0,X,Y,tmp_res);
    free(tmp_res);
    return l_inv;

}

/***** Computer the inverted U^(-1) matrix of upper diagonal block using rectrtri_upper() *****/
double ** get_inv_u(double **a, int xs, int ys, int X, int Y)
{
    double ** u_inv=allocate(X,Y);
    double ** tmp_res=allocate(X,Y);
    int i,j;
    for (i=0; i<X; i++)
        for (j=0; j<Y; j++) {
            if (i<=j)
                u_inv[i][j]=a[i+xs][j+ys];
            else u_inv[i][j]=0;
        }
    rectrtri_upper(u_inv,0,0,X,Y,tmp_res);
    free(tmp_res);
    return u_inv;

}


double ** allocate(int X,int Y)
{
    double ** array;
    array=(double**)calloc(X,sizeof(double*));
    int i;
    for (i=0; i<X; i++)
        array[i]=(double*)calloc(Y,sizeof(double));
    return array;
}

void input(double ** a, int X, int Y)
{
    int i,j;
    for (i=0; i<X; i++)
        for (j=0; j<Y; j++)
            a[i][j]=((double)rand()/1000000.0);
}

void print(double ** a, int N)
{
    int i,j;
    for (i=0; i<N; i++) {
        for (j=0; j<N; j++)
            printf("%.2lf ",a[i][j]);
        printf("\n");
    }
}

/***** Matrix multiplication of an upper triangular matrix with a full matrix *****/
void mm_upper(double ** a1, int x_1, int y_1, double ** a2, int x_2, int y_2, double ** res, int x_3, int y_3, int M, int N, int P)
{
    int i,j,k;
    double sum=0;
    for (i=0; i<M; i++)
        for (j=0; j<P; j++) {
            for (k=0; k<=j; k++)
                sum=sum+a1[i+x_1][k+y_1]*a2[k+x_2][j+y_2];
            up_res[i+x_3-M][j]=sum;
            sum=0;
        }
    for (i=0; i<M; i++)
        for (j=0; j<P; j++)
            res[i+x_3][j+y_3]=up_res[i+x_3-M][j];
}

/***** Matrix multiplication of a lower triangular matrix with a full matrix *****/
void mm_lower(double ** a1, int x_1, int y_1, double ** a2, int x_2, int y_2, double ** res, int x_3, int y_3, int M, int N, int P)
{
    int i,j,k;
    double sum=0;

    for (i=0; i<M; i++)
        for (j=0; j<P; j++) {
            for (k=0; k<=i; k++)
                sum=sum+a1[i+x_1][k+y_1]*a2[k+x_2][j+y_2];
            low_res[i][j+y_3-P]=sum;
            sum=0;
        }
    for (i=0; i<M; i++)
        for (j=0; j<P; j++)
            res[i+x_3][j+y_3]=low_res[i][j+y_3-P];

}

void mm_update(double ** a1, int x_1, int y_1, double ** a2, int x_2, int y_2, double ** res, int x_3, int y_3, int M, int N, int P)
{
    int i,j,k;
    double sum=0;

    for (i=0; i<M; i++)
        for (j=0; j<P; j++) {
            for (k=0; k<N; k++)
                sum+=a1[i+x_1][k+y_1]*a2[k+x_2][j+y_2];
            res[i+x_3][j+y_3]-=sum;
            sum=0;
        }
}

/***** Matrix multiplication: sign=0 --> res=a1*a2, sign=1 --> res=-a1*a2, op = 0 --> res=a1*a2, op = 1 --> res=res+a1*a2  *****/
void mm(double ** a1,int x_1,int y_1, double ** a2, int x_2, int y_2 , double ** res, int x_3, int y_3, double ** tmp_res, int M, int N, int P, int sign)
{
    int i,j,k;
    double sum=0;

    for (i=0; i<M; i++)
        for (j=0; j<P; j++) {
            for (k=0; k<N; k++)
                sum=sum+a1[i+x_1][k+y_1]*a2[k+x_2][j+y_2];
            tmp_res[i+x_3][j+y_3]=sum;
            sum=0;
        }

    for (i=0; i<M; i++)
        for (j=0; j<P; j++) {
            for (k=0; k<N; k++)
                sum=sum-a1[i+x_1][k+y_1]*a2[k+x_2][j+y_2];
            tmp_res[i+x_3][j+y_3]=sum;
            sum=0;
        }

    if (sign==0) {
        for (i=0; i<M; i++)
            for (j=0; j<P; j++)
                res[i+x_3][j+y_3]=tmp_res[i+x_3][j+y_3];
    } else if (sign==1) {
        for (i=0; i<M; i++)
            for (j=0; j<P; j++)
                res[i+x_3][j+y_3]=-tmp_res[i+x_3][j+y_3];
    }
}


/**** Recursive matrix inversion of a lower triangular matrix *****/
void rectrtri_lower(double ** a, int x_s,int y_s, int N, int M, double ** tmp_res)
{
    int n,m;
    if (N==1 && M==1)
        a[x_s][y_s]=1.0/a[x_s][y_s];
    else {
        n=N/2;
        m=M/2;
        cilk_spawn rectrtri_lower(a,x_s,y_s,n,m, tmp_res);
        cilk_spawn rectrtri_lower(a,x_s+n,y_s+m,N-n,M-m, tmp_res);
        cilk_sync;
        mm(a,x_s+n,y_s,a,x_s,y_s,a,x_s+n,y_s,tmp_res,N-n,m,m,0);
        mm(a,x_s+n,y_s+m,a,x_s+n,y_s,a,x_s+n,y_s,tmp_res,N-n,M-m,m,1);
    }
}

/***** Recursive matrix inversion of an upper triangular matrix *****/
void rectrtri_upper(double ** a, int x_s,int y_s, int N, int M, double ** tmp_res)
{
    int n,m;
    if (N==1 && M==1)
        a[x_s][y_s]=1.0/a[x_s][y_s];
    else {
        n=N/2;
        m=M/2;

        cilk_spawn rectrtri_upper(a,x_s,y_s,n,m, tmp_res);
        cilk_spawn rectrtri_upper(a,x_s+n,y_s+m,N-n,M-m, tmp_res);
        cilk_sync;
        mm(a,x_s,y_s+m,a,x_s+n,y_s+m,a,x_s,y_s+m,tmp_res,n,M-m,M-m,0);
        mm(a,x_s,y_s,a,x_s,y_s+m,a,x_s,y_s+m,tmp_res, n,m,M-m,1);
    }

}

