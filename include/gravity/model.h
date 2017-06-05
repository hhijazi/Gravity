//
//  model.hpp
//  Gravity
//
//  Created by Hijazi, Hassan (Data61, Canberra City) on 6/5/17.
//
//

#ifndef model_hpp
#define model_hpp

#include <stdio.h>
#include <Gravity/constraint.h>
#include <map>
#include <unordered_set>
#include <math.h>
#include <vector>
#include <thread>
#ifdef USE_IPOPT
#include <coin/IpIpoptApplication.hpp>
#include <coin/IpTNLP.hpp>
#endif
#ifdef USE_GUROBI
#include <gurobi_c++.h>
#endif
#ifdef USE_BONMIN
#include <coin/BonTMINLP.hpp>
#endif

using namespace std;

class Model {
    
protected:
    string*                         _name;
    vector<shared_ptr<func_>>       _functions;

public:
    unsigned                        _nnz_g; /* Number of non zeros in the Jacobian */
    unsigned                        _nnz_h; /* Number of non zeros in the Hessian */
    vector<var_*>                   _vars; /**< Sorted map pointing to all variables contained in this model */
    vector<Constraint*>             _cons; /**< Sorted map (increasing index) pointing to all constraints contained in this model */
    func_*                          _obj; /** Objective function */
    ObjectiveType                   _objt; /** Minimize or maximize */
    
    /** Constructor */
    //@{
    Model();
    //@}
    
    /* Destructor */
    ~Model();
    
    /* Accessors */
    
    int get_nb_vars() const;
    
    int get_nb_cons() const;
    
    int get_nb_nnz_g() const;
    
    int get_nb_nnz_h() const;
    
    bool has_var(const var_& v) const;
    
    
    /* Modifiers */
    
    void add_var(const var_& v);
    var_* get_var(const string& vname) const;
    Constraint* get_constraint(const string& name) const;
    
    void del_var(const var_& v);
    void add_constraint(const Constraint& c);
    void add_on_off(const Constraint& c, var<bool>& on);
    void add_on_off(var<>& v, var<bool>& on);
    
    void add_on_off_McCormick(string name, var<>& v, var<>& v1, var<>& v2, var<bool>& on);
    void add_McCormick(string name, var<>& v, var<>& v1, var<>& v2);
    
    
    void del_constraint(const Constraint& c);
    void set_objective(const func_& f);
    void set_objective_type(ObjectiveType);
    void check_feasible(const double* x);
    void fill_in_var_bounds(double* x_l ,double* x_u);
    void fill_in_var_init(double* x);
    void fill_in_cstr_bounds(double* g_l ,double* g_u);
    void fill_in_obj(const double* x , double& res);
    void fill_in_grad_obj(const double* x , double* res);
    void fill_in_cstr(const double* x , double* res, bool new_x);
    void fill_in_jac(const double* x , double* res, bool new_x);
    void fill_in_jac_nnz(int* iRow , int* jCol);
    void fill_in_hess(const double* x , double obj_factor, const double* lambda, double* res, bool new_x);
    void eval_funcs_parallel(const double* x , int start, int end);
    void fill_in_hess_nnz(int* iRow , int* jCol);
#ifdef USE_IPOPT
    void fill_in_var_linearity(Ipopt::TNLP::LinearityType* var_types);
    void fill_in_cstr_linearity(Ipopt::TNLP::LinearityType* const_types);
#endif
    
#ifdef USE_BONMIN
    void fill_in_var_types(Bonmin::TMINLP::VariableType* var_types);
#endif
    void solve();
    friend std::vector<int> bounds(int parts, int mem);
    
    /* Operators */
    
    
    
    /* Output */
    void print_functions() const;
    void print() const;
    void print_solution() const;
    void print_constraints() const;
    
};




#endif /* model_hpp */
