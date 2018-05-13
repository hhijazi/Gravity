//
//  Global.hpp
//  projects
//
//  Created by Guanglei Wang on 5/3/18.
//
//
#ifndef Global_hpp
#define Global_hpp

#define _USE_MATH_DEFINES
#include <math.h>

#include "Partition.hpp"


class Global {
public:
    PowerNet* grid;
    Net* chordal;
    Partition* P_;
    int Num_parts; //  spatial decomposition
    int Num_time; // time decomposition

    // Schedule Parameters
    param<Real> rate_ramp;
    param<Real> rate_switch;
    param<int> min_up;
    param<int> min_down;
    param<Real> cost_up;
    param<Real> cost_down;
    param<Real> Pg_initial;
    param<bool> On_off_initial;
    // Variables 
    vector<var<Real>> Pg;
    vector<var<Real>> Pg2; // new var introduced for the perspective formulation.
    vector<var<Real>> Qg;

    //Lifted variables.
    vector<var<Real>> R_Xij;
    vector<var<Real>> Im_Xij;
    vector<var<Real>> Xii;
    // Commitment variables
    //vector<var<bool>> On_off; 
    vector<var<>> On_off; 
    //vector<var<Real>> Start_up;
    //vector<var<Real>> Shut_down;
    //vector<var<bool>> Start_up;
    //vector<var<bool>> Shut_down;
    vector<var<>> Start_up;
    vector<var<>> Shut_down;
    // sol
    vector<param<bool>> On_off_sol_; 
    vector<param<Real>> Pg_sol_;
    //vector<param<Real>> Start_up_sol_;
    //vector<param<Real>> Shut_down_sol_;
    vector<param<bool>> Start_up_sol_;
    vector<param<bool>> Shut_down_sol_;
    param<Real> Im_Xij_sol_; 
    param<Real> R_Xij_sol_; 
    param<Real> Xii_sol_; 
    // power flow vars;
    vector<var<Real>> Pf_from;
    vector<var<Real>> Qf_from;
    vector<var<Real>> Pf_to;
    vector<var<Real>> Qf_to;

   // multipliers time
    param<Real> lambda_up; // inter temporal: start up and shut down constraints
    param<Real> lambda_down;
    param<Real> zeta_up; // ramping constraints
    param<Real> zeta_down;
    param<Real> mu; // dual of min up down constraints
    param<Real> mu_up;
    param<Real> mu_down;
    bool include_min_updown_ =false;
    
    // multipliers spatial
    param<Real> R_lambda_;
    param<Real> Im_lambda_;
    param<Real> lambda_;
    // vals of each subproblem
    vector<double> Sub_;
    // SOCP constraints
    vector<Constraint> SOC_;
    vector<shared_ptr<Constraint>> SOC_outer_;
    // cuts due to Chenchen convex hull.
    
    //Constructors
    Global();
    Global(PowerNet*, int parts, int T);
    ~Global();
    
    // modifiers.
    std::vector<std::vector<string>> get_3dmatrix_index(Net* net, const std::vector<std::vector<Node*>> bag, int t);
    std::vector<std::vector<string>> get_3ddiagon_index(const std::vector<std::vector<Node*>> bag, int t);
    vector<param<>> signs(Net& net, const std::vector<std::vector<Node*>>& bags);
    
    
    double getdual_relax_time_(bool include);
    double solve_sdpcut_opf_();
    double LR_bound_time_(bool included_min_up_down);
    double Upper_bound_sequence_(bool included_min_up_down);
    double Subproblem_time_(int l);
    double Subproblem_upper_time_(int l);
    
    void add_var_Sub_time(Model&, int t);
    void add_obj_Sub_time(Model&, int t);
    void add_obj_Sub_upper_time(Model&, int t);
    void add_SOCP_Sub_time(Model&, int t);
    void add_SOCP_chord_Sub_time(Model&, int t);

    void add_SOCP_Outer_Sub_time(Model&, int t);
    void add_KCL_Sub_time(Model&, int t);
    void add_thermal_Sub_time(Model& , int t);
    void add_perspective_OnOff_Sub_time(Model&, int t);
    void add_MC_upper_Sub_time(Model&, int t);

    void add_MC_intertemporal_Sub_upper_time(Model&, int t);
    void add_OnOff_Sub_upper_time(Model&, int t);
    void add_Ramp_Sub_upper_time(Model&, int t);
    void add_minupdown_Sub_upper_time(Model&, int t);
    
    void add_SOCP_chen_(Model&, int); // need to test..
    // Extended formulation of SOCP
    //vector<shared_ptr<Constraint>> BenNem_SOCP_(Model& model, int k, int t);
    void add_BenNem_SOCP_time(Model& model, int k, int t);
    void add_SDP_S_(Model& model, vector<int> indices, int t); //indices are non rank-1 second order submatrix.   
    void add_SDP_LP_(Model& model, vector<int> indices, int t); //indices are non rank-1 second order submatrix.   
    void add_3d_cuts_static(Model& model, int);
    void add_3d_cuts_violation(Model& model, vector<int> indices, int);

    // check rank 1 constraint.
    vector<int> check_rank1_constraint_(Model& Sub, int t);

    double getdual_relax_spatial();
    double LR_bound_spatial_();
    double Subproblem_spatial_(int l);
};
#endif /* Global_hpp */

