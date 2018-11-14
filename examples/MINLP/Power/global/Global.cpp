//
//  Global.cpp
//  projects
//
//  Created by Guanglei on 5/3/18.
//
//

#include "global.hpp"
#include <algorithm>
#include <queue>

Global::Global() {
  rate_ramp.set_name("rate_ramp");
  rate_switch.set_name("rate_switch");
  min_up.set_name("min_up");
  min_down.set_name("min_down");
  cost_up.set_name("cost_up");
  cost_down.set_name("cost_down");
  min_up = 2;
  min_down = 2;
  cost_up = 50;
  cost_down = 30;
};
Global::~Global() { delete grid; };

Global::Global(PowerNet* net, int parts, unsigned int T) {
  grid = net;
  // chordal = grid->get_chordal_extension();
  grid->get_tree_decomp_bags();
  grid->update_net();
  // grid->update_update_bus_pairs_chord(chordal);
  P_ = new Partition();
  Num_parts = parts;
  if (Num_parts >= 1 && Num_parts < grid->nodes.size()) {
    P_->get_ncut(*grid, Num_parts);
  } else {
    throw std::invalid_argument(
        "please input a valid partition size (>=1 and <= graph size)!");
  }

  R_lambda_.set_name("R_lambda");
  Im_lambda_.set_name("Im_lambda");
  lambda_.set_name("lambda");
  R_lambda_.initialize_all(0);
  Im_lambda_.initialize_all(0);
  lambda_.initialize_all(0);

  Num_time = T;
  rate_ramp.set_name("rate_ramp");
  rate_switch.set_name("rate_switch");
  min_up.set_name("min_up");
  min_down.set_name("min_down");
  cost_up.set_name("cost_up");
  cost_down.set_name("cost_down");
  On_off_initial.set_name("On_off_initial");
  Pg_initial.set_name("Pg_initial");
  // parameters
  min_up = 2;
  min_down = 2;
  cost_up = 50;
  cost_down = 30;
  for (auto g : grid->gens) {
    rate_ramp(g->_name) = max(grid->pg_min(g->_name).getvalue(),
                              1 * grid->pg_max(g->_name).getvalue());
    rate_switch(g->_name) = max(grid->pg_min(g->_name).getvalue(),
                                1 * grid->pg_max(g->_name).getvalue());
  }
  min_up = 2;
  min_down = 2;
  cost_up = 50;
  cost_down = 30;
  grid->time_expand(T);
  rate_ramp.time_expand(T);
  rate_switch.time_expand(T);
  // set initial state or read the initial state
  for (auto g : grid->gens) {
    Pg_initial(g->_name) = 0;
    On_off_initial(g->_name) = 0;
  }

  auto bus_pairs = grid->get_bus_pairs();
  auto bus_pairs_chord = grid->get_bus_pairs_chord();
  for (int t = 0; t < T; t++) {
    // var<double> pgt("Pg_" + to_string(t), grid->pg_min, grid->pg_max);
    var<double> pgt("Pg_" + to_string(t), grid->pg_min.in_at(grid->gens, t),
                    grid->pg_max.in_at(grid->gens, t));
    var<double> qgt("Qg_" + to_string(t), grid->qg_min.in_at(grid->gens, t),
                    grid->qg_max.in_at(grid->gens, t));
    var<double> pg2t(
        "Pg2_" + to_string(t),
        non_neg_);  // new var introduced for the perspective formulation.
    Pg.push_back(pgt);
    Pg2.push_back(pg2t);
    Qg.push_back(qgt);
  }
  // Lifted variables.
  for (int t = 0; t < T; t++) {
    var<double> R_Xijt(
        "R_Xij" + to_string(t), grid->wr_min.in_at(bus_pairs_chord, t),
        grid->wr_max.in_at(bus_pairs_chord, t));  // real part of Wij
    var<double> Im_Xijt("Im_Xij" + to_string(t),
                        grid->wi_min.in_at(bus_pairs_chord, t),
                        grid->wi_max.in_at(bus_pairs_chord, t));
    var<double> Xiit("Xii" + to_string(t), grid->w_min.in_at(grid->nodes, t),
                     grid->w_max.in_at(grid->nodes, t));
    R_Xijt.initialize_all(1.0);
    Xiit.initialize_all(1.001);
    // R_Xij.push_back(R_Xijt.in_at(bus_pairs_chord, t));
    // Im_Xij.push_back(Im_Xijt.in_at(bus_pairs_chord, t));
    // Xii.push_back(Xiit.in_at(grid->nodes, t));
    R_Xij.push_back(R_Xijt);
    Im_Xij.push_back(Im_Xijt);
    Xii.push_back(Xiit);
  }
  for (int t = 0; t < T; t++) {
    var<double> Pf_fromt("Pf_from" + to_string(t),
                         grid->S_max.in_at(grid->arcs, t));
    var<double> Qf_fromt("Qf_from" + to_string(t),
                         grid->S_max.in_at(grid->arcs, t));
    var<double> Pf_tot("Pf_to" + to_string(t),
                       grid->S_max.in_at(grid->arcs, t));
    var<double> Qf_tot("Qf_to" + to_string(t),
                       grid->S_max.in_at(grid->arcs, t));
    Pf_from.push_back(Pf_fromt);
    Qf_from.push_back(Qf_fromt);
    Pf_to.push_back(Pf_tot);
    Qf_to.push_back(Qf_tot);
  }

  // Commitment variables
  for (int t = 0; t < T; t++) {
    // var<bool>  On_offt("On_off_" + to_string(t), 0, 1);
    var<double> On_offt("On_off_" + to_string(t), 0, 1);
    // var<double>  Start_upt("Start_up_" + to_string(t));
    // var<double>  Shut_downt("Shut_down_" + to_string(t));
    // var<bool>  Start_upt("Start_up_" + to_string(t));
    // var<bool>  Shut_downt("Shut_down_" + to_string(t));
    var<double> Start_upt("Start_up_" + to_string(t), 0, 1);
    var<double> Shut_downt("Shut_down_" + to_string(t), 0, 1);
    // On_off.push_back(On_offt.in_at(grid->gens, t-1));
    // Start_up.push_back(Start_upt.in_at(grid->gens, t));
    // Shut_down.push_back(Shut_downt.in_at(grid->gens, t));
    On_off.push_back(On_offt);
    Start_up.push_back(Start_upt);
    Shut_down.push_back(Shut_downt);
  }
  On_off_sol_.resize(T);
  Pg_sol_.resize(T);
  Start_up_sol_.resize(T);
  Shut_down_sol_.resize(T);
  Im_Xij_sol_.set_name("Im_Xij_sol_");
  R_Xij_sol_.set_name("R_Xij_sol_");
  Xii_sol_.set_name("Xii_sol_");
  // multipliers.
  lambda_up.set_name("lambda_up");
  lambda_down.set_name("lambda_down");
  zeta_up.set_name("zeta_up");
  zeta_down.set_name("zeta_down");
  mu.set_name("mu");
  mu_up.set_name("mu_up");
  mu_down.set_name("mu_down");

  lambda_up.in(grid->gens, T);
  lambda_down.in(grid->gens, T);
  zeta_up.in(grid->gens, T);
  zeta_down.in(grid->gens, T);

  lambda_down.initialize_all(0);
  zeta_up.initialize_all(0);
  zeta_down.initialize_all(0);
  mu.in(grid->gens, T);
  mu_up.in(grid->gens, T);
  mu_down.in(grid->gens, T);
  Sub_.resize(T);
  SOC_.resize(T);
  SOC_outer_.resize(T);
  Sdpcut_outer_.resize(T);
}

double Global::solve_sdpcut_opf_() {
  const auto bus_pairs = grid->get_bus_pairs();
  Model ACOPF("ACOPF Model");
  for (int t = 0; t < Num_time; t++) {
    add_var_Sub_time(ACOPF, t);
  }
  /**  Objective */
  func_ obj;
  // auto obj = product(grid->c1, Pg[0]) + product(grid->c2, power(Pg[0],2)) +
  // sum(grid->c0);
  for (int t = 0; t < 1; t++) {
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + "," + to_string(t);
        obj += grid->c1(name) * Pg[t](name) +
               grid->c2(name) * Pg[t](name) * Pg[t](name) + grid->c0(name);
      }
    }
  }

  ACOPF.set_objective(min(obj));
  for (int t = 0; t < Num_time; t++) {
    // add_SOCP_Sub_time(ACOPF, t);
    add_SOCP_chord_Sub_time(ACOPF, t);
    add_3d_cuts_static(ACOPF, t);
  }

  for (int t = 0; t < Num_time; t++) {
    add_KCL_Sub_time(ACOPF, t);
    indices ind = indices(to_string(t));
    Constraint Thermal_Limit_from("Thermal_Limit_from" + to_string(t));
    Thermal_Limit_from = power(Pf_from[t], 2) + power(Qf_from[t], 2);
    Thermal_Limit_from <= power(grid->S_max, 2);
    ACOPF.add_constraint(Thermal_Limit_from.in(grid->arcs, ind));
    // ACOPF.add_constraint(Thermal_Limit_from.in_at(grid->arcs, t));

    Constraint Thermal_Limit_to("Thermal_Limit_to" + to_string(t));
    Thermal_Limit_to = power(Pf_to[t], 2) + power(Qf_to[t], 2);
    Thermal_Limit_to <= power(grid->S_max, 2);
    //ACOPF.add_constraint(Thermal_Limit_to.in(grid->arcs));
    ACOPF.add_constraint(Thermal_Limit_to.in(grid->arcs, ind));

    Constraint PAD_UB("PAD_UB_" + to_string(t));
    PAD_UB = Im_Xij[t] - grid->tan_th_max * R_Xij[t];
    //ACOPF.add_constraint(PAD_UB.in(bus_pairs) <= 0);
    ACOPF.add_constraint(PAD_UB.in(bus_pairs, ind) <= 0);

    Constraint PAD_LB("PAD_LB_" + to_string(t));
    PAD_LB = Im_Xij[t] - grid->tan_th_min * R_Xij[t];
    //ACOPF.add_constraint(PAD_LB.in(bus_pairs) >= 0);
    ACOPF.add_constraint(PAD_LB.in(bus_pairs, ind) >= 0);
  }

  /* Solver selection */
  bool relax = true;
  int output = 5;
  solver cpx_acuc(ACOPF, ipopt);
  double tol = 10e-6;
  cpx_acuc.run(output, relax, tol);
  cout << "the continuous relaxation bound is: " << ACOPF._obj_val << endl;
  return ACOPF._obj_val;
}

double Global::solve_sdpcut_opf_outer() {
  const auto bus_pairs = grid->get_bus_pairs();
  Model ACOPF("ACOPF Model");
  for (int t = 0; t < Num_time; t++) {
    add_var_Sub_time(ACOPF, t);
  }
  /**  Objective */
  func_ obj;
  // auto obj = product(grid->c1, Pg[0]) + product(grid->c2, power(Pg[0],2)) +
  // sum(grid->c0);
  for (int t = 0; t < 1; t++) {
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + "," + to_string(t);
        obj += grid->c1(name) * Pg[t](name) +
               grid->c2(name) * Pg[t](name) * Pg[t](name) + grid->c0(name);
      }
    }
  }

  ACOPF.set_objective(min(obj));
  for (int t = 0; t < Num_time; t++) {
    // add_SOCP_Sub_time(ACOPF, t);
    add_SOCP_chord_Sub_time(ACOPF, t);
    // add_3d_cuts_static(ACOPF,t);
    // add_Sdpcut_Outer_Sub_time(ACOPF, t);
    // add_Sdpcut_Second_order_Sub_time(ACOPF, t);
  }

  for (int t = 0; t < Num_time; t++) {
    indices ind = indices(to_string(t));
    add_KCL_Sub_time(ACOPF, t);
    Constraint Thermal_Limit_from("Thermal_Limit_from" + to_string(t));
    Thermal_Limit_from = power(Pf_from[t], 2) + power(Qf_from[t], 2);
    Thermal_Limit_from <= power(grid->S_max, 2);
    ACOPF.add_constraint(Thermal_Limit_from.in(grid->arcs, ind));

    Constraint Thermal_Limit_to("Thermal_Limit_to" + to_string(t));
    Thermal_Limit_to = power(Pf_to[t], 2) + power(Qf_to[t], 2);
    Thermal_Limit_to <= power(grid->S_max, 2);
    ACOPF.add_constraint(Thermal_Limit_to.in(grid->arcs, ind));

    Constraint PAD_UB("PAD_UB_" + to_string(t));
    PAD_UB = Im_Xij[t] - grid->tan_th_max * R_Xij[t];
    ACOPF.add_constraint(PAD_UB.in(bus_pairs, ind) <= 0);

    Constraint PAD_LB("PAD_LB_" + to_string(t));
    PAD_LB = Im_Xij[t] - grid->tan_th_min * R_Xij[t];
    ACOPF.add_constraint(PAD_LB.in(bus_pairs, ind) >= 0);
  }

  /* Solver selection */
  bool relax = true;
  int output = 5;
  solver cpx_acuc(ACOPF, ipopt);
  double tol = 10e-6;
  cpx_acuc.run(output, relax, tol);
  cout << "the continuous relaxation bound is: " << ACOPF._obj_val << endl;
  return ACOPF._obj_val;
}

double Global::getdual_relax_time_(bool include) {
  include_min_updown_ = include;
  const auto bus_pairs = grid->get_bus_pairs();
  Model ACUC("ACUC Model");
  for (int t = 0; t < Num_time; t++) {
    add_var_Sub_time(ACUC, t);
  }
  /* Construct the objective function*/
  func_ obj;
  for (int t = 0; t < Num_time; t++) {
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + "," + to_string(t);
        obj += grid->c1(name) * Pg[t](name) +
               grid->c2(name) * Pg[t](name) * Pg[t](name) +
               grid->c0(name) * On_off[t](name);
        obj += cost_up.getvalue() * Start_up[t](name) +
               cost_down.getvalue() * Shut_down[t](name);
      }
    }
  }
  ACUC.set_objective(min(obj));
  for (int t = 0; t < Num_time; t++) {
    add_SOCP_Sub_time(ACUC, t);
    //add_SOCP_chord_Sub_time(ACUC, t);
//    if (grid->add_3d_nlin) {
//      add_3d_cuts_static(ACUC, t);
//    }
  }

  for (int t = 0; t < Num_time; t++) {
    add_KCL_Sub_time(ACUC, t);
    add_thermal_Sub_time(ACUC, t);
  }
  for (int t = 0; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        Constraint MC1("Inter_temporal_MC1_" + to_string(t) + "," + g->_name);
        Constraint MC2("Inter_temporal_MC2_" + to_string(t) + "," + g->_name);
        string name = g->_name + "," + to_string(t);
        string name1 = g->_name + "," + to_string(t - 1);
        if (t > 0) {
          MC1 = On_off[t](name) - On_off[t - 1](name1) - Start_up[t](name);
          MC2 = On_off[t - 1](name1) - On_off[t](name) - Shut_down[t](name);
        } else {
          MC1 = On_off[0](name) - On_off_initial(name1) - Start_up[0](name);
          MC2 =
              -1 * On_off[0](name) - Shut_down[0](name) + On_off_initial(name1);
        }
        ACUC.add_constraint(MC1 <= 0);
        ACUC.add_constraint(MC2 <= 0);
      }
    }
  }
  for (int t = 0; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        Constraint OnOffStartupShutdown("OnOffStartupShutdown_" + to_string(t) +
                                        "," + g->_name);
        string name = g->_name + "," + to_string(t);
        string name1 = g->_name + "," + to_string(t - 1);
        if (t > 0) {
          OnOffStartupShutdown = On_off[t](name) - On_off[t - 1](name1) -
                                 Start_up[t](name) + Shut_down[t](name);
        } else {
          OnOffStartupShutdown = On_off[t](name) - On_off_initial(name1) -
                                 Start_up[t](name) + Shut_down[t](name);
        }
        ACUC.add_constraint(OnOffStartupShutdown == 0);
      }
    }
  }

  if (include_min_updown_) {
    // Min-up constraints  4b
    for (int t = min_up.getvalue() - 1; t < Num_time; t++) {
      Constraint Min_Up("Min_Up_constraint_" + to_string(t));
      for (int l = t - min_up.getvalue() + 1; l < t + 1; l++) {
        Min_Up += Start_up[l].in_at(grid->gens, l);
      }
      Min_Up -= On_off[t].in_at(grid->gens, t);
      ACUC.add_constraint(Min_Up <= 0);
    }
    // 4c
    for (int t = min_down.getvalue() - 1; t < Num_time; t++) {
      Constraint Min_Down("Min_Down_constraint_" + to_string(t));
      for (int l = t - min_down.getvalue() + 1; l < t + 1; l++) {
        Min_Down += Shut_down[l].in_at(grid->gens, l);
      }
      Min_Down -= 1 - On_off[t].in_at(grid->gens, t);
      ACUC.add_constraint(Min_Down <= 0);
    }
  } else {
    for (int t = 0; t < Num_time; t++) {
      add_MC_upper_Sub_time(ACUC, t);
    }
  }
  for (int t = 1; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        Constraint Ramp_up("Ramp_up_constraint_" + to_string(t) + "," +
                           g->_name);
        Constraint Ramp_down("Ramp_down_constraint_" + to_string(t) + "," +
                             g->_name);
        string name = g->_name + "," + to_string(t);
        string name1 = g->_name + "," + to_string(t - 1);
        Ramp_up = Pg[t](name) - Pg[t - 1](name1) -
                  rate_ramp.getvalue() * On_off[t - 1](name1) -
                  rate_switch.getvalue() * (1 - On_off[t - 1](name1));
        Ramp_down = Pg[t - 1](name1) - Pg[t](name) -
                    rate_ramp.getvalue() * On_off[t](name) -
                    rate_switch.getvalue() * (1 - On_off[t](name));
        ACUC.add_constraint(Ramp_up <= 0);
        ACUC.add_constraint(Ramp_down <= 0);
      }
    }
  }
  // t =0, we have ramp up constraint.
  for (auto& g : grid->gens) {
    Constraint Ramp_up("Ramp_up_constraint_0," + g->_name);
    string name = g->_name + ",0";
    Ramp_up = Pg[0](name) - Pg_initial(g->_name) -
              rate_ramp(name) * On_off_initial(g->_name);
    Ramp_up -= rate_switch(name) * (1 - On_off_initial(g->_name));
    ACUC.add_constraint(Ramp_up <= 0);

    Constraint Ramp_down("Ramp_down_constraint_0," + g->_name);
    Ramp_down = -1 * Pg[0](name) + Pg_initial.in(g->_name);
    Ramp_down -= rate_ramp(name) * On_off[0](name);
    Ramp_down -= rate_switch(name) * (1 - On_off[0](name));
    ACUC.add_constraint(Ramp_down <= 0);
  }
  /* Solver selection */
  bool relax = true;
  int output = 5;
  bool useipopt = true;
  SolverType st;
  if (grid->add_3d_nlin) {
    st = ipopt;
  } else {
    st = cplex;
    useipopt = false;
  }
  solver cpx_acuc(ACUC, st);
  // useipopt = true;
  // solver cpx_acuc(ACUC, ipopt);
  // solver cpx_acuc(ACUC, cplex);
  double tol = 10e-6;
  cpx_acuc.run(output, relax, tol);
  cout << "the continuous relaxation bound is: " << ACUC._obj_val << endl;
  for (int t = 1; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        auto MC1 = ACUC.get_constraint("Inter_temporal_MC1_" + to_string(t) +
                                       "," + g->_name);
        auto MC2 = ACUC.get_constraint("Inter_temporal_MC2_" + to_string(t) +
                                       "," + g->_name);
        string name = g->_name + "," + to_string(t);
        lambda_up(name) = abs(MC1->_dual.at(0));
        lambda_down(name) = abs(MC2->_dual.at(0));
        // lambda_up(name) = (MC1->_dual.at(0));
        // lambda_down(name) = (MC2->_dual.at(0));
        Debug("dual of  lambda_up_" << name << " " << lambda_up(name).eval()
                                    << endl);
        Debug("dual of  lambda_down_" << name << " " << lambda_down(name).eval()
                                      << endl);
      }
    }
  }
  // we do not relax first step constraint
  for (int t = 1; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        auto Ramp_up = ACUC.get_constraint("Ramp_up_constraint_" +
                                           to_string(t) + "," + g->_name);
        string name = g->_name + "," + to_string(t);
        zeta_up(name) = abs(Ramp_up->_dual.at(0));
        // zeta_up(name) = (Ramp_up->_dual.at(0));

        auto Ramp_down = ACUC.get_constraint("Ramp_down_constraint_" +
                                             to_string(t) + "," + g->_name);
        zeta_down(name) = abs(Ramp_down->_dual.at(0));
        // zeta_down(name) = (Ramp_down->_dual.at(0));

        Debug("dual of  zeta_up_" << name << " " << zeta_up(name).eval()
                                  << endl);
        Debug("dual of  zeta_down_" << name << " " << zeta_up(name).eval()
                                    << endl);
      }
    }
  }
  // we do not relax first step constraint
  for (int t = 1; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        auto OOSS = ACUC.get_constraint("OnOffStartupShutdown_" + to_string(t) +
                                        "," + g->_name);
        string name = g->_name + "," + to_string(t);
        if (useipopt) {
          mu(name) = OOSS->_dual.at(0);  // when positive and when negative?
        } else {
          mu(name) = -OOSS->_dual.at(0);  // when positive and when negative?
        }
        Debug("mu: " << mu(name).eval() << endl);
      }
    }
  }

  if (include_min_updown_) {
    if (min_up.getvalue() > 1) {
      for (int t = min_up.getvalue() - 1; t < Num_time; t++) {
        auto Min_up = ACUC.get_constraint("Min_Up_constraint_" + to_string(t));
        auto Min_down =
            ACUC.get_constraint("Min_Down_constraint_" + to_string(t));
        int l = 0;
        for (auto& g : grid->gens) {
          if (g->_active) {
            string name = g->_name + "," + to_string(t);
            mu_up(name) = abs(Min_up->_dual.at(l));
            mu_down(name) = abs(Min_down->_dual.at(l));
            l += 1;
          }
        }
      }
    }
  }
  check_rank1_constraint_(ACUC, 0);
  return ACUC._obj_val;
}

double Global::getdual_relax_spatial() {
  R_Xij.clear();
  Im_Xij.clear();
  Xii.clear();
  Pg.clear();
  Pg2.clear();
  Qg.clear();
  On_off.clear();
  Start_up.clear();
  Shut_down.clear();

  for (int c = 0; c < Num_parts; c++) {
    var<double> bag_Xii("Xii_" + to_string(c),
                        grid->w_min.in(P_->bag_bus_union_out[c], Num_time),
                        grid->w_max.in(P_->bag_bus_union_out[c], Num_time));
    Xii.push_back(bag_Xii);
    var<double> bag_R_Xij(
        "R_Xij_" + to_string(c),
        grid->wr_min.in(P_->bag_bus_pairs_union[c], Num_time),
        grid->wr_max.in(P_->bag_bus_pairs_union[c], Num_time));
    var<double> bag_Im_Xij(
        "Im_Xij_" + to_string(c),
        grid->wi_min.in(P_->bag_bus_pairs_union[c], Num_time),
        grid->wi_max.in(P_->bag_bus_pairs_union[c], Num_time));
    R_Xij.push_back(bag_R_Xij);
    Im_Xij.push_back(bag_Im_Xij);

    var<double> bag_Pg("Pg_" + to_string(c),
                       grid->pg_min.in(P_->bag_gens[c], Num_time),
                       grid->pg_max.in(P_->bag_gens[c], Num_time));
    var<double> bag_Qg("Qg_" + to_string(c),
                       grid->qg_min.in(P_->bag_gens[c], Num_time),
                       grid->qg_max.in(P_->bag_gens[c], Num_time));
    var<double> bag_Pg2(
        "Pg2_" + to_string(c),
        non_neg_);  // new var introduced for the perspective formulation.
    Pg.push_back(bag_Pg);
    Pg2.push_back(bag_Pg2.in(P_->bag_gens[c], Num_time));
    Qg.push_back(bag_Qg);

    // var<bool>  bag_Onoff("On_off_" + to_string(c));
    // var<bool>  bag_Up("Start_up_" + to_string(c));
    // var<bool>  bag_Down("Shut_down_" + to_string(c));
    var<> bag_Onoff("On_off_" + to_string(c));
    var<> bag_Up("Start_up_" + to_string(c));
    var<> bag_Down("Shut_down_" + to_string(c));
    // var<double>  bag_Up("Start_up_" + to_string(c));
    // var<double>  bag_Down("Shut_down_" + to_string(c));

    On_off.push_back(bag_Onoff);
    Start_up.push_back(bag_Up);
    Shut_down.push_back(bag_Down);
  }

  Model ACUC("ACUC Model");
  for (int c = 0; c < Num_parts; c++) {
    ACUC.add_var(Pg[c].in(P_->bag_gens[c], Num_time));
    ACUC.add_var(Pg2[c].in(P_->bag_gens[c], Num_time));
    ACUC.add_var(Qg[c].in(P_->bag_gens[c], Num_time));
    ACUC.add_var(Start_up[c].in(P_->bag_gens[c], Num_time));
    ACUC.add_var(Shut_down[c].in(P_->bag_gens[c], Num_time));
    ACUC.add_var(On_off[c].in(P_->bag_gens[c], Num_time));
    /// ACUC.add_var(On_off[c].in(P_->bag_gens[c], -1,  Num_time));
    ACUC.add_var(Xii[c].in(P_->bag_bus_union_out[c], Num_time));
    Xii[c].initialize_all(1.001);
    ACUC.add_var(R_Xij[c].in(P_->bag_bus_pairs_union[c], Num_time));
    R_Xij[c].initialize_all(1.0);
    ACUC.add_var(Im_Xij[c].in(P_->bag_bus_pairs_union[c], Num_time));
  }
  // power flow vars are treated as auxiliary vars.
  vector<var<double>> Pf_from;
  vector<var<double>> Qf_from;
  vector<var<double>> Pf_to;
  vector<var<double>> Qf_to;
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_arcs_union[c].size() > 0) {
      var<double> bag_Pf_from(
          "Pf_from" + to_string(c),
          grid->S_max.in(P_->bag_arcs_union_out[c], Num_time));
      var<double> bag_Qf_from(
          "Qf_from" + to_string(c),
          grid->S_max.in(P_->bag_arcs_union_out[c], Num_time));
      var<double> bag_Pf_to("Pf_to" + to_string(c),
                            grid->S_max.in(P_->bag_arcs_union_in[c], Num_time));
      var<double> bag_Qf_to("Qf_to" + to_string(c),
                            grid->S_max.in(P_->bag_arcs_union_in[c], Num_time));
      Pf_from.push_back(bag_Pf_from);
      Qf_from.push_back(bag_Qf_from);
      Pf_to.push_back(bag_Pf_to);
      Qf_to.push_back(bag_Qf_to);
      ACUC.add_var(bag_Pf_from.in(P_->bag_arcs_union_out[c], Num_time));
      ACUC.add_var(bag_Qf_from.in(P_->bag_arcs_union_out[c], Num_time));
      ACUC.add_var(bag_Pf_to.in(P_->bag_arcs_union_in[c], Num_time));
      ACUC.add_var(bag_Qf_to.in(P_->bag_arcs_union_in[c], Num_time));
    } else {
      var<double> empty("empty");
      empty.set_size(0);
      Pf_from.push_back(empty);
      Pf_to.push_back(empty);
      Qf_from.push_back(empty);
      Qf_to.push_back(empty);
    }
  }
  /* Construct the objective function*/
  func_ obj;
  for (int c = 0; c < Num_parts; c++) {
    for (auto g : P_->bag_gens[c]) {
      for (int t = 0; t < Num_time; t++) {
        if (g->_active) {
          string name = g->_name + "," + to_string(t);
          obj += grid->c1(name) * Pg[c](name) +
                 grid->c2(name) * Pg[c](name) * Pg[c](name) +
                 grid->c0(name) * On_off[c](name);
          // obj += grid.c1(name)*Pg[c](name)+ grid.c2(name)*Pg2[c](name)
          // +grid.c0(name)*On_off[c](name);
          obj += cost_up * Start_up[c](name) + cost_down * Shut_down[c](name);
        }
      }
    }
  }
  ACUC.set_objective(min(obj));
  indices idx = indices(1, Num_time);
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_bus_pairs_union_directed[c].size() > 0) {
      Constraint SOC("SOC_" + to_string(c));
      SOC = power(R_Xij[c], 2) + power(Im_Xij[c], 2) -
            Xii[c].from() * Xii[c].to();
      ACUC.add_constraint(SOC.in(P_->bag_bus_pairs_union_directed[c], idx) <=
                          0);
    }
  }
  /* perspective formulation of Pg^2 */
  // for (int c = 0; c < nbparts; c++) {
  //    if (P_->bag_gens[c].size() >0){
  //        Constraint Perspective_OnOff_Pg2("Perspective_OnOff_Pg2_" +
  //        to_string(c)); Perspective_OnOff_Pg2 = power(Pg[c], 2) -
  //        Pg2[c]*On_off[c]; ACUC.add(Perspective_OnOff_Pg2.in(P_->bag_gens[c],
  //        Num_time) <= 0);
  //    }
  //}
  // KCL
  for (int c = 0; c < Num_parts; c++) {
    Constraint KCL_P("KCL_P" + to_string(c));
    Constraint KCL_Q("KCL_Q" + to_string(c));
    KCL_P = sum(Pf_from[c].out_arcs()) + sum(Pf_to[c].in_arcs()) -
            sum(Pg[c].in_gens()) + grid->gs * Xii[c] + grid->pl;
    ACUC.add_constraint(KCL_P.in(P_->bag_bus[c], idx) == 0);
    KCL_Q = sum(Qf_from[c].out_arcs()) + sum(Qf_to[c].in_arcs()) + grid->ql -
            sum(Qg[c].in_gens()) - grid->bs * Xii[c];
    ACUC.add_constraint(KCL_Q.in(P_->bag_bus[c], idx) == 0);

    Constraint Flow_P_From("Flow_P_From_" + to_string(c));
    Flow_P_From = Pf_from[c] - (grid->g_ff * Xii[c].from() +
                                grid->g_ft * R_Xij[c].in_pairs() +
                                grid->b_ft * Im_Xij[c].in_pairs());
    ACUC.add_constraint(Flow_P_From.in(P_->bag_arcs_union_out[c], idx) == 0);

    Constraint Flow_P_Num_timeo("Flow_P_Num_timeo" + to_string(c));
    Flow_P_Num_timeo = Pf_to[c] - (grid->g_tt * Xii[c].to() +
                                   grid->g_tf * R_Xij[c].in_pairs() -
                                   grid->b_tf * Im_Xij[c].in_pairs());
    ACUC.add_constraint(Flow_P_Num_timeo.in(P_->bag_arcs_union_in[c], idx) ==
                        0);

    Constraint Flow_Q_From("Flow_Q_From" + to_string(c));
    Flow_Q_From = Qf_from[c] - (grid->g_ft * Im_Xij[c].in_pairs() -
                                grid->b_ff * Xii[c].from() -
                                grid->b_ft * R_Xij[c].in_pairs());
    ACUC.add_constraint(Flow_Q_From.in(P_->bag_arcs_union_out[c], idx) == 0);

    Constraint Flow_Q_Num_timeo("Flow_Q_Num_timeo" + to_string(c));
    Flow_Q_Num_timeo = Qf_to[c] + (grid->b_tt * Xii[c].to() +
                                   grid->b_tf * R_Xij[c].in_pairs() +
                                   grid->g_tf * Im_Xij[c].in_pairs());
    ACUC.add_constraint(Flow_Q_Num_timeo.in(P_->bag_arcs_union_in[c], idx) ==
                        0);

    Constraint Num_timehermal_Limit_from("Num_timehermal_Limit_from" +
                                         to_string(c));
    Num_timehermal_Limit_from = power(Pf_from[c], 2) + power(Qf_from[c], 2);
    Num_timehermal_Limit_from <= power(grid->S_max, 2);
    ACUC.add_constraint(
        Num_timehermal_Limit_from.in(P_->bag_arcs_union_out[c], idx));

    Constraint Num_timehermal_Limit_to("Num_timehermal_Limit_to" +
                                       to_string(c));
    Num_timehermal_Limit_to = power(Pf_to[c], 2) + power(Qf_to[c], 2);
    Num_timehermal_Limit_to <= power(grid->S_max, 2);
    ACUC.add_constraint(
        Num_timehermal_Limit_to.in(P_->bag_arcs_union_in[c], idx));
  }
  ///* Phase Angle Bounds constraints */
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_bus_pairs_union_directed[c].size() > 0) {
      Constraint PAD_UB("PAD_UB_" + to_string(c));
      PAD_UB = Im_Xij[c] - grid->tan_th_max * R_Xij[c];
      ACUC.add_constraint(PAD_UB.in(P_->bag_bus_pairs_union_directed[c], idx) <=
                          0);

      Constraint PAD_LB("PAD_LB_" + to_string(c));
      PAD_LB = Im_Xij[c] - grid->tan_th_min * R_Xij[c];
      ACUC.add_constraint(PAD_LB.in(P_->bag_bus_pairs_union_directed[c], idx) >=
                          0);
    }
  }
  // COMMINum_timeMENNum_time CONSNum_timeRAINNum_timeS
  // Inter-temporal constraints 3a, 3d
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      for (int t = 0; t < Num_time; t++) {
        Constraint MC1("MC1_" + to_string(c) + "," + to_string(t));
        Constraint MC2("MC2_" + to_string(c) + "," + to_string(t));
        MC1 = On_off[c].in_at(P_->bag_gens[c], t) -
              On_off[c].in_at(P_->bag_gens[c], t - 1) -
              Start_up[c].in_at(P_->bag_gens[c], t);
        MC2 = On_off[c].in_at(P_->bag_gens[c], t - 1) -
              On_off[c].in_at(P_->bag_gens[c], t) -
              Shut_down[c].in_at(P_->bag_gens[c], t);
        ACUC.add_constraint(MC1 <= 0);
        ACUC.add_constraint(MC2 <= 0);
      }
    }
  }
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      for (int t = 0; t < Num_time; t++) {
        Constraint OnOffStartupShutdown("OnOffStartupShutdown_" + to_string(t));
        OnOffStartupShutdown = On_off[c].in_at(P_->bag_gens[c], t) -
                               On_off[c].in_at(P_->bag_gens[c], t - 1) -
                               Start_up[c].in_at(P_->bag_gens[c], t) +
                               Shut_down[c].in_at(P_->bag_gens[c], t);
        ACUC.add_constraint(OnOffStartupShutdown == 0);
      }
    }
  }
  // 4b
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      for (int t = min_up.getvalue() - 1; t < Num_time; t++) {
        Constraint Min_Up("Min_Up_constraint" + to_string(c) + "_" +
                          to_string(t));
        for (int l = t - min_up.getvalue() + 1; l < t + 1; l++) {
          Min_Up += Start_up[c].in_at(P_->bag_gens[c], l);
        }
        Min_Up -= On_off[c].in_at(P_->bag_gens[c], t);
        ACUC.add_constraint(Min_Up <= 0);
      }
    }
  }
  // 4c
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      for (int t = min_down.getvalue() - 1; t < Num_time; t++) {
        Constraint Min_Down("Min_Down_constraint_" + to_string(c) + "_" +
                            to_string(t));
        for (int l = t - min_down.getvalue() + 1; l < t + 1; l++) {
          Min_Down += Shut_down[c].in_at(P_->bag_gens[c], l);
        }
        Min_Down -= 1 - On_off[c].in_at(P_->bag_gens[c], t);
        ACUC.add_constraint(Min_Down <= 0);
      }
    }
  }
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      Constraint Production_P_LB("Production_P_LB_" + to_string(c));
      Constraint Production_P_UB("Production_P_UB_" + to_string(c));
      Constraint Production_Q_LB("Production_Q_LB_" + to_string(c));
      Constraint Production_Q_UB("Production_Q_UB_" + to_string(c));
      // 5A
      Production_P_UB = Pg[c] - grid->pg_max * On_off[c];
      Production_P_LB = Pg[c] - grid->pg_min * On_off[c];
      ACUC.add_constraint(Production_P_UB.in(P_->bag_gens[c], idx) <= 0);
      ACUC.add_constraint(Production_P_LB.in(P_->bag_gens[c], idx) >= 0);

      Production_Q_UB = Qg[c] - grid->qg_max * On_off[c];
      Production_Q_LB = Qg[c] - grid->qg_min * On_off[c];
      ACUC.add_constraint(Production_Q_UB.in(P_->bag_gens[c], idx) <= 0);
      ACUC.add_constraint(Production_Q_LB.in(P_->bag_gens[c], idx) >= 0);
    }
  }
  // 5C
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      for (int t = 1; t < Num_time; t++) {
        Constraint Ramp_up("Ramp_up_constraint_" + to_string(c) + "_" +
                           to_string(t));
        Constraint Ramp_down("Ramp_down_constraint_" + to_string(c) + "_" +
                             to_string(t));
        Ramp_up = Pg[c].in_at(P_->bag_gens[c], t);
        Ramp_up -= Pg[c].in_at(P_->bag_gens[c], t - 1);
        Ramp_up -= rate_ramp.in_at(P_->bag_gens[c], t) *
                   On_off[c].in_at(P_->bag_gens[c], t - 1);
        Ramp_up -= rate_switch.in_at(P_->bag_gens[c], t) *
                   (1 - On_off[c].in_at(P_->bag_gens[c], t));

        Ramp_down = Pg[c].in_at(P_->bag_gens[c], t - 1);
        Ramp_down -= Pg[c].in_at(P_->bag_gens[c], t);
        Ramp_down -= rate_ramp.in_at(P_->bag_gens[c], t) *
                     On_off[c].in_at(P_->bag_gens[c], t);
        Ramp_down -= rate_switch.in_at(P_->bag_gens[c], t) *
                     (1 - On_off[c].in_at(P_->bag_gens[c], t - 1));

        ACUC.add_constraint(Ramp_up <= 0);
        ACUC.add_constraint(Ramp_down <= 0);
      }
      Constraint Ramp_up("Ramp_up_constraint0" + to_string(c));
      Ramp_up = Pg[c].in_at(P_->bag_gens[c], 0) -
                Pg_initial.in(P_->bag_gens[c]) -
                rate_ramp.in_at(P_->bag_gens[c], 0) *
                    On_off_initial.in(P_->bag_gens[c]);
      Ramp_up -= rate_switch.in_at(P_->bag_gens[c], 0) *
                 (1 - On_off_initial.in(P_->bag_gens[c]));
      ACUC.add_constraint(Ramp_up <= 0);

      Constraint Ramp_down("Ramp_down_constraint0");
      Ramp_down =
          -1 * Pg[c].in_at(P_->bag_gens[c], 0) + Pg_initial.in(P_->bag_gens[c]);
      Ramp_down -= rate_ramp.in_at(P_->bag_gens[c], 0) *
                   On_off[c].in_at(P_->bag_gens[c], 0);
      Ramp_down -= rate_switch.in_at(P_->bag_gens[c], 0) *
                   (1 - On_off[c].in_at(P_->bag_gens[c], 0));
      ACUC.add_constraint(Ramp_down <= 0);
    }
  }
  // set the initial state of generators.
  for (int c = 0; c < Num_parts; c++) {
    if (P_->bag_gens[c].size() > 0) {
      Constraint gen_initial("gen_initial_" + to_string(c));
      gen_initial += On_off[c].in_at(P_->bag_gens[c], -1) -
                     On_off_initial.in(P_->bag_gens[c]);
      ACUC.add_constraint(gen_initial == 0);
    }
  }
  // Linking constraints
  for (const auto& a : P_->G_part.arcs) {
    Constraint Link_R("link_R_" + a->_name);
    Link_R = R_Xij[a->_src->_id].in_pairs() - R_Xij[a->_dest->_id].in_pairs();
    ACUC.add_constraint(Link_R.in(a->_intersection_clique, idx) == 0);

    Constraint Link_Im("link_Im_" + a->_name);
    Link_Im =
        Im_Xij[a->_src->_id].in_pairs() - Im_Xij[a->_dest->_id].in_pairs();
    ACUC.add_constraint(Link_Im.in(a->_intersection_clique, idx) == 0);

    Constraint Link_Xii("link_Xii_" + a->_name);
    Link_Xii = Xii[a->_src->_id].to() - Xii[a->_dest->_id].to();
    ACUC.add_constraint(Link_Xii.in(a->_intersection_clique, idx) == 0);
  }
  /* Solver selection */
  // solver cpx_acuc(ACUC, cplex);
  solver cpx_acuc(ACUC, ipopt);
  bool relax = true;
  int output = 1;
  double tol = 1e-6;
  cpx_acuc.run(output, relax, tol);
  cout << "the continuous relaxation bound is: " << ACUC._obj_val << endl;
  for (const auto& a : P_->G_part.arcs) {
    shared_ptr<Constraint> consR = ACUC.get_constraint("link_R_" + a->_name);
    shared_ptr<Constraint> consIm = ACUC.get_constraint("link_Im_" + a->_name);
    shared_ptr<Constraint> cons = ACUC.get_constraint("link_Xii_" + a->_name);
    unsigned id = 0;
    for (unsigned t = 0; t < Num_time; t++) {
      for (auto& line : a->_intersection_clique) {
        string name = line->_name + "," + to_string(t);
        //                auto cR = (*consR)(name);
        //                auto cIm = (*consIm)(name);
        //                auto  c= (*cons)(name);
        R_lambda_(name) = -consR->_dual.at(id);
        Im_lambda_(name) = -consIm->_dual.at(id);
        lambda_(name) = -cons->_dual.at(id);
        id++;
        //                DebugOff("R_lambda_" << -cR._dual.at(0) << endl);
        //                DebugOff("Im_lambda_" << -cIm._dual.at(0)<< endl);
        //                DebugOff("lambda_" << -c._dual.at(0)<< endl);
      }
    }
  }
  return ACUC._obj_val;
}

void Global::add_var_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  const auto bus_pairs = grid->get_bus_pairs();
  const auto bus_pairs_chord = grid->get_bus_pairs_chord();
  // Sub.add_var(Pg[t]);
  //Sub.add_var(Pg[t].in_at(grid->gens, t));
  //Sub.add_var(Pg2[t].in_at(grid->gens, t));
  //Sub.add_var(Qg[t].in_at(grid->gens, t));
  //Sub.add_var(On_off[t].in_at(grid->gens, t));
  Sub.add_var(Pg[t].in(grid->gens, ind));
  Sub.add_var(Pg2[t].in(grid->gens, ind));
  Sub.add_var(Qg[t].in(grid->gens, ind));
  Sub.add_var(On_off[t].in(grid->gens, ind));
  // Sub.add_var(On_off[t]);
  // Sub.add_var(Start_up[t]);
  //Sub.add_var(Start_up[t].in_at(grid->gens, t));
  Sub.add_var(Start_up[t].in(grid->gens, ind));
  // Sub.add_var(Shut_down[t]);
  //Sub.add_var(Shut_down[t].in_at(grid->gens, t));
  //Sub.add_var(Xii[t].in_at(grid->nodes, t));
  //Sub.add_var(R_Xij[t].in_at(bus_pairs_chord, t));
  //Sub.add_var(Im_Xij[t].in_at(bus_pairs_chord, t));
  Sub.add_var(Shut_down[t].in(grid->gens, ind));
  Sub.add_var(Xii[t].in(grid->nodes, ind));
  Sub.add_var(R_Xij[t].in(bus_pairs_chord, ind));
  Sub.add_var(Im_Xij[t].in(bus_pairs_chord, ind));
  Xii[t].initialize_all(1.001);
  R_Xij[t].initialize_all(1.0);
  // power flow
  //Sub.add_var(Pf_from[t].in_at(grid->arcs, t));
  //Sub.add_var(Qf_from[t].in_at(grid->arcs, t));
  //Sub.add_var(Pf_to[t].in_at(grid->arcs, t));
  //Sub.add_var(Qf_to[t].in_at(grid->arcs, t));

  Sub.add_var(Pf_from[t].in(grid->arcs, ind));
  Sub.add_var(Qf_from[t].in(grid->arcs, ind));
  Sub.add_var(Pf_to[t].in(grid->arcs, ind));
  Sub.add_var(Qf_to[t].in(grid->arcs, ind));
}

void Global::add_obj_Sub_time(gravity::Model& Sub, int t) {
  func_ obj;
  indices ind = indices(0, Num_time);
  if (t == 0) {
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + ",0";
        string name1 = g->_name + ",1";
        obj +=
            (grid->c1(name) + zeta_down(name1) - zeta_up(name1)) * Pg[t](name) +
            grid->c2(name) * Pg2[t](name);
        // obj += (grid->c1(name))*Pg[t](name) + grid->c2(name)*Pg2[t](name);
        obj += (grid->c0(name) + lambda_down(name1) - lambda_up(name1) +
                zeta_up(name1) * rate_switch(name1) -
                zeta_up(name1) * rate_ramp(name1) - mu(name1)) *
               On_off[t](name);
        // obj +=(grid->c0(name) + lambda_down(name1) - lambda_up(name1)
        // -mu(name1))*On_off[t](name);
        obj += cost_up.getvalue() * Start_up[t](name) +
               cost_down.getvalue() * Shut_down[t](name);
        if (include_min_updown_) {
          string name2 = g->_name + "," + to_string(min_up.getvalue() - 1);
          if (min_up.getvalue() > 1) {
            obj += mu_up(name2) * Start_up[t](name);
          }
          if (min_down.getvalue() > 1) {
            obj += mu_down(name2) * Shut_down[t](name);
          }
        }
      }
    }
  } else if (t == Num_time - 1) {
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + "," + to_string(t);
        obj +=
            (grid->c1(name) + zeta_up(name) - zeta_down(name)) * Pg[t](name) +
            grid->c2(name) * Pg2[t](name);
        // obj += (grid->c1(name))*Pg[t](name) + grid->c2(name)*Pg2[t](name);
        // obj += (grid->c0(name)+lambda_up(name)
        // -lambda_down(name)+mu(name))*On_off[t](name);
        obj += (grid->c0(name) + lambda_up(name) - lambda_down(name) -
                zeta_down(name) * rate_ramp(name) +
                zeta_down(name) * rate_switch(name) + mu(name)) *
               On_off[t](name);
        if (include_min_updown_) {
          obj += (mu_down(name) - mu_up(name)) * On_off[t](name);
        }

        // obj += (cost_up-lambda_up(name))*Start_up[t](name);
        // obj += (cost_down-lambda_down(name))*Shut_down[t](name);
        obj += (cost_up - lambda_up(name) - mu(name)) * Start_up[t](name);
        obj += (cost_down - lambda_down(name) + mu(name)) * Shut_down[t](name);
        if (include_min_updown_) {
          if (min_up.getvalue() > 1) {
            obj += mu_up(name) * Start_up[t](name);
          }
          if (min_down.getvalue() > 1) {
            obj += mu_down(name) * Shut_down[t](name);
          }
        }
      }
    }
  } else {
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + "," + to_string(t);
        string name1 = g->_name + "," + to_string(t + 1);
        obj += (grid->c1(name) + zeta_up(name) + zeta_down(name1) -
                zeta_down(name) - zeta_up(name1)) *
                   Pg[t](name) +
               grid->c2(name) * Pg2[t](name);
        // obj += (grid->c1(name))*Pg[t](name)+ grid->c2(name)*Pg2[t](name);
        // obj += (grid->c0(name)+lambda_up(name) -lambda_up(name1) +
        // lambda_down(name1) -lambda_down(name)+ mu(name) -
        // mu(name1))*On_off[t](name);
        obj += (grid->c0(name) + lambda_up(name) - lambda_up(name1) +
                lambda_down(name1) - lambda_down(name) -
                zeta_down(name) * rate_ramp(name) -
                zeta_up(name1) * rate_ramp(name1) +
                zeta_up(name1) * rate_switch(name1) +
                zeta_down(name) * rate_switch(name) + mu(name) - mu(name1)) *
               On_off[t](name);

        if (include_min_updown_) {
          if (t >= min_up.getvalue() - 1) {
            obj -= mu_up(name) * On_off[t + 1](name);
          }
          if (t >= min_down.getvalue() - 1) {
            obj += mu_down(name) * On_off[t + 1](name);
          }
        }
        obj += (cost_up.getvalue() - lambda_up(name) - mu(name)) *
               Start_up[t](name);
        obj += (cost_down.getvalue() - lambda_down(name) + mu(name)) *
               Shut_down[t](name);
        // obj +=
        // (cost_up.getvalue()-lambda_up(name)-mu(name))*Start_up[t](name); obj
        // +=
        // (cost_down.getvalue()-lambda_down(name)+mu(name))*Shut_down[t](name);
        if (include_min_updown_) {
          if (min_up.getvalue() > 1) {
            int start = std::max(min_up.getvalue() - 1, t);
            int end = std::min(min_up.getvalue() + t,
                               (int)Num_time);  // both increase 1.
            for (int l = start; l < end; l++) {
              string name2 = g->_name + "," + to_string(l);
              obj += mu_up(name2) * Start_up[t](name);
            }
          }
          if (min_down.getvalue() > 1) {
            int start = std::max(min_down.getvalue() - 1, t);
            int end = std::min(min_down.getvalue() + t, (int)Num_time);
            for (int l = start; l < end; l++) {
              string name2 = g->_name + "," + to_string(l);
              obj += mu_down(name2) * Shut_down[t](name);
            }
          }
        }
      }
    }
  }
  Sub.set_objective(min(obj));
}

void Global::add_obj_Sub_upper_time(gravity::Model& Sub, int t) {
  // great
  func_ obj;
  for (auto g : grid->gens) {
    if (g->_active) {
      string name = g->_name + "," + to_string(t);
      obj += grid->c1(name) * Pg[t](name) +
             grid->c2(name) * Pg[t](name) * Pg[t](name) +
             grid->c0(name) * On_off[t](name);
      obj += cost_up.getvalue() * Start_up[t](name) +
             cost_down.getvalue() * Shut_down[t](name);
    }
  }
  Sub.set_objective(min(obj));
}

void Global::add_perspective_OnOff_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  /* Construct the objective function*/
  Constraint Perspective_OnOff_Pg2("Perspective_OnOff_Pg2_");
  Perspective_OnOff_Pg2 = power(Pg[t], 2) - Pg2[t] * On_off[t];
  Sub.add(Perspective_OnOff_Pg2.in(grid->gens, ind) <= 0);
}
void Global::add_SOCP_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  const auto bus_pairs = grid->get_bus_pairs();
  // Constraint SOC("SOC_" + to_string(t));
  SOC_[t] = Constraint("SOC_" + to_string(t));
  SOC_[t] = power(R_Xij[t], 2) + power(Im_Xij[t], 2) - Xii[t].from() * Xii[t].to();
  SOC_outer_[t] = Sub.add_constraint(SOC_[t].in(bus_pairs, ind) <= 0);
  // Sub.add_constraint(SOC_[t].in_at(bus_pairs, t) <= 0);
}

void Global::add_SOCP_chord_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  const auto bus_pairs_chord = grid->get_bus_pairs_chord();
  // Constraint SOC("SOC_" + to_string(t));
  SOC_[t] = Constraint("SOC_" + to_string(t));
  SOC_[t] =
      power(R_Xij[t], 2) + power(Im_Xij[t], 2) - Xii[t].from() * Xii[t].to();
  SOC_outer_[t] = Sub.add_constraint(SOC_[t].in(bus_pairs_chord, ind) <= 0);
  // SOC_[t].in_at(bus_pairs_chord, t).print_expanded();
}

void Global::add_SOCP_Outer_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  // for (auto& pair: bus_pairs){
  Constraint SOC_outer_linear("SOC_outer_linear_" + to_string(t));
  SOC_outer_linear = SOC_outer_[t]->get_outer_app();
  SOC_outer_linear.print_expanded();
  Sub.add_constraint(SOC_outer_linear.in(grid->get_bus_pairs_chord(), ind) <=
                     0);
  // Sub.add_constraint(SOC_outer_linear <= 0);
  //}
}

void Global::add_Sdpcut_Outer_Sub_time(Model& Sub, int t) {
  Constraint Sdpcut_outer_linear("Sdpcut_outer_linear_" + to_string(t));
  Sdpcut_outer_linear = Sdpcut_outer_[t]->get_outer_app();
  Sdpcut_outer_linear.print_expanded();
  Sub.add_constraint(Sdpcut_outer_linear >= 0);
}

void Global::add_Sdpcut_Second_order_Sub_time(Model& Model, int t) {
  Constraint Sdpcut_outer_QP("Sdpcut_outer_QP_" + to_string(t));
  // sdp cut  hessan of 3d sdp..
  Sdpcut_outer_QP = Sdpcut_outer_[t]->get_outer_app();
  // Sdpcut_outer_QP = Sdpcut_outer_[t]->get_second_outer_app();
  Sdpcut_outer_QP.print_expanded();
  Model.add_constraint(Sdpcut_outer_QP >= 0);
}

void Global::add_KCL_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  Constraint KCL_P("KCL_P_" + to_string(t));
  Constraint KCL_Q("KCL_Q_" + to_string(t));
  KCL_P = sum(Pf_from[t].out_arcs()) + sum(Pf_to[t].in_arcs()) + grid->pl -
          sum(Pg[t].in_gens()) + grid->gs * Xii[t];
  Sub.add_constraint(KCL_P.in(grid->nodes, ind) == 0);

  KCL_Q = sum(Qf_from[t].out_arcs()) + sum(Qf_to[t].in_arcs()) + grid->ql -
          sum(Qg[t].in_gens()) - grid->bs * Xii[t];
  Sub.add_constraint(KCL_Q.in(grid->nodes, ind) == 0);

  Constraint Flow_P_From("Flow_P_From" + to_string(t));
  Flow_P_From = Pf_from[t] -
                (grid->g_ff * Xii[t].from() + grid->g_ft * R_Xij[t].in_pairs() +
                 grid->b_ft * Im_Xij[t].in_pairs());
  Sub.add_constraint(Flow_P_From.in(grid->arcs, ind) == 0);

  Constraint Flow_P_To("Flow_P_To" + to_string(t));
  Flow_P_To =
      Pf_to[t] - (grid->g_tt * Xii[t].to() + grid->g_tf * R_Xij[t].in_pairs() -
                  grid->b_tf * Im_Xij[t].in_pairs());
  Sub.add_constraint(Flow_P_To.in(grid->arcs, ind) == 0);

  Constraint Flow_Q_From("Flow_Q_From" + to_string(t));
  Flow_Q_From = Qf_from[t] -
                (grid->g_ft * Im_Xij[t].in_pairs() -
                 grid->b_ff * Xii[t].from() - grid->b_ft * R_Xij[t].in_pairs());
  Sub.add_constraint(Flow_Q_From.in(grid->arcs, ind) == 0);

  Constraint Flow_Q_To("Flow_Q_To" + to_string(t));
  Flow_Q_To =
      Qf_to[t] + (grid->b_tt * Xii[t].to() + grid->b_tf * R_Xij[t].in_pairs() +
                  grid->g_tf * Im_Xij[t].in_pairs());
  Sub.add_constraint(Flow_Q_To.in(grid->arcs, ind) == 0);
}
void Global::add_thermal_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  const auto bus_pairs = grid->get_bus_pairs();
  Constraint Thermal_Limit_from("Thermal_Limit_from" + to_string(t));
  Thermal_Limit_from = power(Pf_from[t], 2) + power(Qf_from[t], 2);
  Thermal_Limit_from <= power(grid->S_max, 2);
  Sub.add_constraint(Thermal_Limit_from.in(grid->arcs, ind));

  Constraint Thermal_Limit_to("Thermal_Limit_to" + to_string(t));
  Thermal_Limit_to = power(Pf_to[t], 2) + power(Qf_to[t], 2);
  Thermal_Limit_to <= power(grid->S_max, 2);
  Sub.add_constraint(Thermal_Limit_to.in(grid->arcs, ind));

  Constraint PAD_UB("PAD_UB_" + to_string(t));
  PAD_UB = Im_Xij[t] - grid->tan_th_max * R_Xij[t];
  Sub.add_constraint(PAD_UB.in(bus_pairs, ind) <= 0);

  Constraint PAD_LB("PAD_LB_" + to_string(t));
  PAD_LB = Im_Xij[t] - grid->tan_th_min * R_Xij[t];
  Sub.add_constraint(PAD_LB.in(bus_pairs, ind) >= 0);

  Constraint Production_P_LB("Production_P_LB_" + to_string(t));
  Constraint Production_P_UB("Production_P_UB_" + to_string(t));
  Constraint Production_Q_LB("Production_Q_LB_" + to_string(t));
  Constraint Production_Q_UB("Production_Q_UB_" + to_string(t));
  Production_P_UB = Pg[t] - grid->pg_max * On_off[t];
  Production_P_LB = Pg[t] - grid->pg_min * On_off[t];
  Sub.add_constraint(Production_P_UB.in(grid->gens, ind) <= 0);
  Sub.add_constraint(Production_P_LB.in(grid->gens, ind) >= 0);

  Production_Q_UB = Qg[t] - grid->qg_max * On_off[t];
  Production_Q_LB = Qg[t] - grid->qg_min * On_off[t];
  Sub.add_constraint(Production_Q_UB.in(grid->gens, ind) <= 0);
  Sub.add_constraint(Production_Q_LB.in(grid->gens, ind) >= 0);
}

void Global::add_MC_upper_Sub_time(Model& Sub, int t) {
  indices ind = indices(to_string(t));
  Constraint MC_upper1("MC_upper1_constraint_" + to_string(t));
  param<bool> On_off_val("On_off_val");
  // On_off_val = On_off[t+1];
  MC_upper1 = Start_up[t] - On_off[t];
  Sub.add_constraint(MC_upper1.in(grid->gens, ind) <= 0);

  Constraint MC_upper2("MC_upper2_constraint_" + to_string(t));
  MC_upper2 = Shut_down[t] - 1 + On_off[t];
  Sub.add_constraint(MC_upper1.in(grid->gens, ind) <= 0);
}

void Global::add_MC_intertemporal_Sub_upper_time(Model& ACUC, int t) {
  for (auto& g : grid->gens) {
    if (g->_active) {
      Constraint MC1("Inter_temporal_MC1_" + to_string(t) + "," + g->_name);
      Constraint MC2("Inter_temporal_MC2_" + to_string(t) + "," + g->_name);
      if (t > 0) {
        string name = g->_name + "," + to_string(t);
        string name1 = g->_name + "," + to_string(t - 1);
        MC1 = On_off[t](name) - On_off_sol_[t - 1](name1) - Start_up[t](name);
        MC2 = -1 * On_off[t](name) - Shut_down[t](name) +
              On_off_sol_[t - 1](name1);
      } else {
        string name = g->_name + ",0";
        MC1 = On_off[t](name) - On_off_initial(g->_name) - Start_up[t](name);
        MC2 = -1 * On_off[t](name) + On_off_initial(g->_name) -
              Shut_down[t](name);
      }
      ACUC.add_constraint(MC1 <= 0);
      ACUC.add_constraint(MC2 <= 0);
    }
  }
}
void Global::add_OnOff_Sub_upper_time(Model& ACUC, int t) {
  for (auto& g : grid->gens) {
    if (g->_active) {
      Constraint OnOffStartupShutdown("OnOffStartupShutdown_" + to_string(t) +
                                      "," + g->_name);
      string name = g->_name + "," + to_string(t);
      if (t > 0) {
        string name1 = g->_name + "," + to_string(t - 1);
        OnOffStartupShutdown = On_off[t](name) - On_off_sol_[t - 1](name1) -
                               Start_up[t](name) + Shut_down[t](name);
      } else {
        OnOffStartupShutdown = On_off[t](name) - On_off_initial(g->_name) -
                               Start_up[t](name) + Shut_down[t](name);
      }
      ACUC.add_constraint(OnOffStartupShutdown == 0);
    }
  }
}

void Global::add_Ramp_Sub_upper_time(Model& ACUC, int t) {
  if (t > 0) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        Constraint Ramp_up("Ramp_up_constraint_" + to_string(t) + "," +
                           g->_name);
        Constraint Ramp_down("Ramp_down_constraint_" + to_string(t) + "," +
                             g->_name);
        string name = g->_name + "," + to_string(t);
        string name1 = g->_name + "," + to_string(t - 1);
        Ramp_up = Pg[t](name) - Pg_sol_[t - 1](name1) -
                  rate_ramp.getvalue() * On_off_sol_[t - 1](name1) -
                  rate_switch.getvalue() * (1 - On_off_sol_[t - 1](name1));
        Ramp_down = -1 * Pg[t](name) + Pg_sol_[t - 1](name1) -
                    rate_ramp.getvalue() * On_off[t](name) -
                    rate_switch.getvalue() * (1 - On_off[t](name));
        ACUC.add_constraint(Ramp_up <= 0);
        ACUC.add_constraint(Ramp_down <= 0);
      }
    }
  } else {
    for (auto& g : grid->gens) {
      if (g->_active) {
        Constraint Ramp_up("Ramp_up_constraint_0," + g->_name);
        string name = g->_name + ",0";
        Ramp_up = Pg[0](name) - Pg_initial(g->_name) -
                  rate_ramp(name) * On_off_initial(g->_name);
        Ramp_up -= rate_switch(name) * (1 - On_off_initial(g->_name));
        ACUC.add_constraint(Ramp_up <= 0);

        Constraint Ramp_down("Ramp_down_constraint_0," + g->_name);
        Ramp_down = -1 * Pg[0](name) + Pg_initial.in(g->_name);
        Ramp_down -= rate_ramp(name) * On_off[0](name);
        Ramp_down -= rate_switch(name) * (1 - On_off[0](name));
        ACUC.add_constraint(Ramp_down <= 0);
      }
    }
  }
}

void Global::add_minupdown_Sub_upper_time(Model& ACUC, int t) {
  if (t >= min_up.getvalue() - 1 && t < Num_time) {
    Constraint Min_Up("Min_Up_constraint_" + to_string(t));
    Min_Up -= On_off[t].in_at(grid->gens, t);
    for (int l = t - min_up.getvalue() + 1; l < t; l++) {
      Min_Up += Start_up_sol_[l].in_at(grid->gens, l);
    }
    Min_Up += Start_up[t].in_at(grid->gens, t);
    ACUC.add_constraint(Min_Up <= 0);
  }
  if (t >= min_up.getvalue() - 1 && t < Num_time) {
    Constraint Min_Down("Min_Down_constraint_" + to_string(t));
    Min_Down -= 1 - On_off[t].in_at(grid->gens, t);
    for (int l = t - min_down.getvalue() + 1; l < t; l++) {
      Min_Down += Shut_down_sol_[l].in_at(grid->gens, l);
    }
    Min_Down += Shut_down[t].in_at(grid->gens, t);
    ACUC.add_constraint(Min_Down <= 0);
  }
}

double Global::Subproblem_time_(int t) {
  // Grid Parameters
  const auto bus_pairs = grid->get_bus_pairs();
  Model Sub("Sub_" + to_string(t));
  add_var_Sub_time(Sub, t);
  add_obj_Sub_time(Sub, t);
  add_perspective_OnOff_Sub_time(Sub, t);
  // add_SOCP_Sub_time(Sub, t);
  add_SOCP_chord_Sub_time(Sub, t);
  // add_SOCP_Outer_Sub_time(Sub, t);
  // add_SOCP_Sub_time(Sub, t);

  if (grid->add_3d_nlin) {
    // add_3d_cuts_static(Sub,t);
    // add_3d_cuts_static(Sub,t);
    // add_Sdpcut_Outer_Sub_time(Sub, t);
  }
  add_KCL_Sub_time(Sub, t);
  add_thermal_Sub_time(Sub, t);
  add_MC_upper_Sub_time(Sub, t);
  if (t == 0) {
    for (auto& g : grid->gens) {
      if (g->_active) {
        Constraint MC1("Inter_temporal_MC1_0," + g->_name);
        Constraint MC2("Inter_temporal_MC2_0," + g->_name);
        string name = g->_name + ",0";
        MC1 = On_off[t](name) - On_off_initial(g->_name) - Start_up[t](name);
        MC2 = -1 * On_off[t](name) + On_off_initial(g->_name) -
              Shut_down[t](name);
        Sub.add_constraint(MC1 <= 0);
        Sub.add_constraint(MC2 <= 0);
      }
    }

    for (auto& g : grid->gens) {
      Constraint Ramp_up("Ramp_up_constraint_0," + g->_name);
      string name = g->_name + ",0";
      Ramp_up = Pg[t](name) - Pg_initial(g->_name) -
                rate_ramp(name) * On_off_initial(g->_name);
      Ramp_up -= rate_switch(name) * (1 - On_off_initial(g->_name));
      Sub.add_constraint(Ramp_up <= 0);

      Constraint Ramp_down("Ramp_down_constraint_0," + g->_name);
      Ramp_down = -1 * Pg[t](name) + Pg_initial.in(g->_name);
      Ramp_down -= rate_ramp(name) * On_off[t](name);
      Ramp_down -= rate_switch(name) * (1 - On_off[t](name));
      Sub.add_constraint(Ramp_down <= 0);
    }
    for (auto& g : grid->gens) {
      Constraint OnOffStartupShutdown("OnOffStartupShutdown_" + to_string(t) +
                                      "," + g->_name);
      string name = g->_name + "," + to_string(t);
      OnOffStartupShutdown = On_off[t](name) - On_off_initial(g->_name) -
                             Start_up[t](name) + Shut_down[t](name);
      Sub.add_constraint(OnOffStartupShutdown == 0);
    }
  }
  /* Solver selection */

  SolverType st;
  if (grid->add_3d_nlin) {
    st = ipopt;
  } else {
    st = cplex;
  }
  solver cpx_acuc(Sub, st);

  bool relax = true;
  int output = 1;
  double tol = 1e-6;
  cpx_acuc.run(output, relax, tol);

  if (t == 0) {
    // fill solution
    // On_off_sol_[t].set_name(" On_off_sol_"+to_string(t)+".in_at_" +
    // to_string(t) + "Gen"); std::string name = On_off[t].in_at(grid->gens,
    // t).get_name(); On_off_sol_[t] =
    // *(param<bool>*)(Sub.get_var("On_off_"+to_string(t)+".in_at_" +
    // to_string(t) + "Gen"));
    On_off_sol_[t].set_name(" On_off_sol_" + to_string(t));
    Start_up_sol_[t].set_name("Start_up_sol_" + to_string(t));
    Shut_down_sol_[t].set_name("Shut_down_sol_" + to_string(t));
    for (auto& g : grid->gens) {
      string name = g->_name + "," + to_string(t);
      On_off_sol_[t](name) = On_off[t](name).eval();
      Start_up_sol_[t](name) = Start_up[t](name).eval();
      Shut_down_sol_[t](name) = Shut_down[t](name).eval();
      Pg_sol_[t](name) = Pg[t](name).eval();
    }
  }
  // check_rank1_constraint_(Sub, t);
  return Sub._obj_val;
}

double Global::Subproblem_upper_time_(int t) {
  const auto bus_pairs = grid->get_bus_pairs();
  Model Sub("Sub_" + to_string(t));
  add_var_Sub_time(Sub, t);
  // add_obj_Sub_time(Sub, t);
  add_obj_Sub_upper_time(Sub, t);
  add_perspective_OnOff_Sub_time(Sub, t);
  add_SOCP_Sub_time(Sub, t);
  add_KCL_Sub_time(Sub, t);
  add_thermal_Sub_time(Sub, t);
  add_MC_upper_Sub_time(Sub, t);
  // add interesting constraints
  add_MC_intertemporal_Sub_upper_time(Sub, t);
  add_OnOff_Sub_upper_time(Sub, t);
  add_Ramp_Sub_upper_time(Sub, t);
  add_minupdown_Sub_upper_time(Sub, t);

  double ub = 0;
  /* Solver selection */
  solver cpx_acuc(Sub, cplex);
  bool relax = false;
  int output = 1;
  double tol = 1e-6;
  cpx_acuc.run(output, relax, tol);
  // fill solution

  On_off_sol_[t].set_name(" On_off_sol_" + to_string(t));
  Start_up_sol_[t].set_name("Start_up_sol_" + to_string(t));
  Shut_down_sol_[t].set_name("Shut_down_sol_" + to_string(t));
  for (auto& g : grid->gens) {
    string name = g->_name + "," + to_string(t);
    On_off_sol_[t](name) = On_off[t](name).eval();
    Start_up_sol_[t](name) = Start_up[t](name).eval();
    Shut_down_sol_[t](name) = Shut_down[t](name).eval();
    Pg_sol_[t](name) = Pg[t](name).eval();
  }
  // Start_up_sol_[t].print(true);
  // Shut_down_sol_[t].print(true);
  // On_off_sol_[t].print(true);
  //    On_off_sol_[t].set_name(" On_off_sol_"+to_string(t));
  //    On_off_sol_[t] =
  //    *(param<bool>*)(Sub.get_var("On_off_"+to_string(t)+".in_at_" +
  //    to_string(t) + "Gen"));
  //    //On_off_sol_[t].print(true);
  //    Start_up_sol_[t] =
  //    *(param<bool>*)(Sub.get_var("Start_up_"+to_string(t)+".in_at_" +
  //    to_string(t) + "Gen"));
  //    //Start_up_sol_[t].print(true);
  //    Shut_down_sol_[t] =
  //    *(param<bool>*)(Sub.get_var("Shut_down_"+to_string(t)+".in_at_" +
  //    to_string(t) + "Gen"));
  //    //Shut_down_sol_[t].print(true);
  //    Pg_sol_[t] = *(param<double>*)(Sub.get_var("Pg_"+to_string(t)+".in_at_"
  //    + to_string(t) + "Gen")); Pg_sol_[t].print(true);
  // check_rank1_constraint_(Sub, t);
  return Sub._obj_val;
}

double Global::LR_bound_time_(bool included_min_up_down) {
  include_min_updown_ = included_min_up_down;
  double LB = 0;
  for (int t = 0; t < Num_time; t++) {
    Sub_[t] = Subproblem_time_(t);
    LB += Sub_[t];
  }
  for (int t = 1; t < Num_time; t++) {
    for (auto& g : grid->gens) {
      string name = g->_name + "," + to_string(t);
      LB -= zeta_down(name).getvalue() * rate_switch(name).getvalue();
      LB -= zeta_up(name).getvalue() * rate_switch(name).getvalue();
    }
  }
  if (include_min_updown_) {
    if (min_down.getvalue() - 1.0 > 0) {
      for (int t = min_down.getvalue() - 1; t < Num_time; t++) {
        for (auto& g : grid->gens) {
          string name = g->_name + "," + to_string(t);
          LB -= mu_down(name).getvalue();
        }
      }
    }
  }
  return LB;
}

double Global::Upper_bound_sequence_(bool included_min_up_down) {
  include_min_updown_ = included_min_up_down;
  On_off_sol_.resize(Num_time);
  Start_up_sol_.resize(Num_time);
  Shut_down_sol_.resize(Num_time);
  double UB = 0;
  for (int t = 0; t < Num_time; t++) {
    if (t >= 1) {
      Sub_[t] = Subproblem_upper_time_(t);
    }
    // else {
    func_ obj;
    for (auto g : grid->gens) {
      if (g->_active) {
        string name = g->_name + "," + to_string(t);
        obj += (grid->c1(name) * Pg_sol_[t](name) +
                grid->c2(name) * Pg_sol_[t](name) * Pg_sol_[t](name) +
                grid->c0(name) * On_off_sol_[t](name));
        obj += cost_up.getvalue() * Start_up_sol_[t](name) +
               cost_down.getvalue() * Shut_down_sol_[t](name);
      }
    }
    Sub_[t] = poly_eval(&obj);
    //}
    UB += Sub_[t];
  }
  return UB;
}

double Global::Subproblem_spatial_(int l) {
  indices ind = indices(1, Num_time);
  assert(Pg.size() == Num_parts);
  Model Subr("Subr");
  Subr.add_var(Pg[l].in(P_->bag_gens[l], Num_time));
  Subr.add_var(Pg2[l].in(P_->bag_gens[l], Num_time));
  Subr.add_var(Qg[l].in(P_->bag_gens[l], Num_time));
  Subr.add_var(On_off[l].in(P_->bag_gens[l], Num_time));
  // Subr.add_var(On_off[l].in(P_->bag_gens[l], -1, Num_time));
  Subr.add_var(Start_up[l].in(P_->bag_gens[l], Num_time));
  Subr.add_var(Shut_down[l].in(P_->bag_gens[l], Num_time));
  Subr.add_var(Xii[l].in(P_->bag_bus_union_out[l], Num_time));
  Xii[l].initialize_all(1.001);
  Subr.add_var(R_Xij[l].in(P_->bag_bus_pairs_union[l], Num_time));
  R_Xij[l].initialize_all(1.0);
  Subr.add_var(Im_Xij[l].in(P_->bag_bus_pairs_union[l], Num_time));
  // power flow
  var<double> Pf_from("Pf_from",
                      grid->S_max.in(P_->bag_arcs_union_out[l], Num_time));
  var<double> Qf_from("Qf_from",
                      grid->S_max.in(P_->bag_arcs_union_out[l], Num_time));
  var<double> Pf_to("Pf_to",
                    grid->S_max.in(P_->bag_arcs_union_in[l], Num_time));
  var<double> Qf_to("Qf_to",
                    grid->S_max.in(P_->bag_arcs_union_in[l], Num_time));
  if (P_->bag_arcs_union[l].size() > 0) {
    Subr.add_var(Pf_from.in(P_->bag_arcs_union_out[l], Num_time));
    Subr.add_var(Qf_from.in(P_->bag_arcs_union_out[l], Num_time));
    Subr.add_var(Pf_to.in(P_->bag_arcs_union_in[l], Num_time));
    Subr.add_var(Qf_to.in(P_->bag_arcs_union_in[l], Num_time));
  }
  /* Construct the objective function*/
  func_ obj;
  for (auto g : P_->bag_gens[l]) {
    if (g->_active) {
      for (int t = 0; t < Num_time; t++) {
        string name = g->_name + "," + to_string(t);
        obj += grid->c1(name) * Pg[l](name) + grid->c2(name) * Pg2[l](name) +
               grid->c0(name) * On_off[l](name);
        obj += cost_up * Start_up[l](name) + cost_down * Shut_down[l](name);
      }
    }
  }
  const auto& bag = P_->G_part.get_node(to_string(l));
  for (const auto& a : bag->get_out()) {
    for (int t = 0; t < Num_time; t++) {
      for (auto& pair : a->_intersection_clique) {
        string name = pair->_name + "," + to_string(t);
        string dest = pair->_dest->_name + "," + to_string(t);
        obj += R_lambda_(name) * R_Xij[l](name) +
               Im_lambda_(name) * Im_Xij[l](name) +
               lambda_(name) * Xii[l](dest);
      }
    }
  }

  for (const auto& a : bag->get_in()) {
    for (unsigned t = 0; t < Num_time; t++) {
      for (auto pair : a->_intersection_clique) {
        string name = pair->_name + "," + to_string(t);
        string dest = pair->_dest->_name + "," + to_string(t);
        obj -= R_lambda_(name) * R_Xij[l](name) +
               Im_lambda_(name) * Im_Xij[l](name) +
               lambda_(name) * Xii[l](dest);
      }
    }
  }
  Subr.min(obj);
  /* perspective formulation of Pg[l]^2 */
  if (P_->bag_gens[l].size() > 0) {
    Constraint Perspective_OnOff_Pg2("Perspective_OnOff_Pg2_");
    Perspective_OnOff_Pg2 = power(Pg[l], 2) - Pg2[l] * On_off[l];
    Subr.add(Perspective_OnOff_Pg2.in(P_->bag_gens[l], ind) <= 0);
  }

  if (P_->bag_bus_pairs_union_directed[l].size() > 0) {
    SOC_[l] = Constraint("SOC_" + to_string(l));
    // SOC =  power(R_Xij[l], 2)+ power(Im_Xij[l], 2)-Xii[l].from()*Xii[l].to()
    // ;
    SOC_[l] =
        power(R_Xij[l], 2) + power(Im_Xij[l], 2) - Xii[l].from() * Xii[l].to();
    // Subr.add_constraint(SOC.in(P_->bag_bus_pairs_union_directed[l], Num_time)
    // <= 0);
    Subr.add_constraint(SOC_[l].in(P_->bag_bus_pairs_union_directed[l], ind) <=
                        0);
  }
  Constraint KCL_P("KCL_P");
  Constraint KCL_Q("KCL_Q");
  KCL_P = sum(Pf_from.out_arcs()) + sum(Pf_to.in_arcs()) + grid->pl -
          sum(Pg[l].in_gens()) + grid->gs * Xii[l];
  Subr.add_constraint(KCL_P.in(P_->bag_bus[l], ind) == 0);
  KCL_Q = sum(Qf_from.out_arcs()) + sum(Qf_to.in_arcs()) + grid->ql -
          sum(Qg[l].in_gens()) - grid->bs * Xii[l];
  Subr.add_constraint(KCL_Q.in(P_->bag_bus[l], ind) == 0);

  Constraint Flow_P_From("Flow_P_From");
  Flow_P_From =
      Pf_from - (grid->g_ff * Xii[l].from() + grid->g_ft * R_Xij[l].in_pairs() +
                 grid->b_ft * Im_Xij[l].in_pairs());
  Subr.add_constraint(Flow_P_From.in(P_->bag_arcs_union_out[l], ind) == 0);

  Constraint Flow_P_indo("Flow_P_ind");
  Flow_P_indo =
      Pf_to - (grid->g_tt * Xii[l].to() + grid->g_tf * R_Xij[l].in_pairs() -
               grid->b_tf * Im_Xij[l].in_pairs());
  Subr.add_constraint(Flow_P_indo.in(P_->bag_arcs_union_in[l], ind) == 0);

  Constraint Flow_Q_From("Flow_Q_From");
  Flow_Q_From =
      Qf_from - (grid->g_ft * Im_Xij[l].in_pairs() -
                 grid->b_ff * Xii[l].from() - grid->b_ft * R_Xij[l].in_pairs());
  Subr.add_constraint(Flow_Q_From.in(P_->bag_arcs_union_out[l], ind) == 0);

  Constraint Flow_Q_indo("Flow_Q_ind");
  Flow_Q_indo =
      Qf_to + (grid->b_tt * Xii[l].to() + grid->b_tf * R_Xij[l].in_pairs() +
               grid->g_tf * Im_Xij[l].in_pairs());
  Subr.add_constraint(Flow_Q_indo.in(P_->bag_arcs_union_in[l], ind) == 0);

  Constraint indhermal_Limit_from("indhermal_Limit_from");
  indhermal_Limit_from = power(Pf_from, 2) + power(Qf_from, 2);
  indhermal_Limit_from <= power(grid->S_max, 2);
  Subr.add_constraint(indhermal_Limit_from.in(P_->bag_arcs_union_out[l], ind));

  Constraint indhermal_Limit_to("indhermal_Limit_to");
  indhermal_Limit_to = power(Pf_to, 2) + power(Qf_to, 2);
  indhermal_Limit_to <= power(grid->S_max, 2);
  Subr.add_constraint(indhermal_Limit_to.in(P_->bag_arcs_union_in[l], ind));
  /* Phase Angle Bounds constraints */
  Constraint PAD_UB("PAD_UB_" + to_string(l));
  PAD_UB = Im_Xij[l] - grid->tan_th_max * R_Xij[l];
  Subr.add_constraint(PAD_UB.in(P_->bag_bus_pairs_union_directed[l], ind) <= 0);

  Constraint PAD_LB("PAD_LB_" + to_string(l));
  PAD_LB = Im_Xij[l] - grid->tan_th_min * R_Xij[l];
  Subr.add_constraint(PAD_LB.in(P_->bag_bus_pairs_union_directed[l], ind) >= 0);
  // COMMIindMENind CONSindRAINindS
  // Inter-temporal constraints 3a, 3d
  if (P_->bag_gens[l].size() > 0) {
    for (int t = 0; t < Num_time; t++) {
      Constraint MC1("Inter_temporal_MC1_" + to_string(t));
      Constraint MC2("Inter_temporal_MC2_" + to_string(t));
      MC1 = On_off[l].in_at(P_->bag_gens[l], t) -
            On_off[l].in_at(P_->bag_gens[l], t - 1) -
            Start_up[l].in_at(P_->bag_gens[l], t);
      MC2 = On_off[l].in_at(P_->bag_gens[l], t - 1) -
            On_off[l].in_at(P_->bag_gens[l], t) -
            Shut_down[l].in_at(P_->bag_gens[l], t);
      Subr.add_constraint(MC1 <= 0);
      Subr.add_constraint(MC2 <= 0);
    }
  }
  if (P_->bag_gens[l].size() > 0) {
    for (int t = 0; t < Num_time; t++) {
      Constraint OnOffStartupShutdown("OnOffStartupShutdown_" + to_string(t));
      OnOffStartupShutdown = On_off[l].in_at(P_->bag_gens[l], t) -
                             On_off[l].in_at(P_->bag_gens[l], t - 1) -
                             Start_up[l].in_at(P_->bag_gens[l], t) +
                             Shut_down[l].in_at(P_->bag_gens[l], t);
      Subr.add_constraint(OnOffStartupShutdown == 0);
    }
  }

  if (P_->bag_gens[l].size() > 0) {
    for (int t = min_up.getvalue() - 1; t < Num_time; t++) {
      Constraint Min_Up("Min_Up_constraint" + to_string(l) + "_" +
                        to_string(t));
      for (int ll = t - min_up.getvalue() + 1; ll < t + 1; ll++) {
        Min_Up += Start_up[l].in_at(P_->bag_gens[l], ll);
      }
      Min_Up -= On_off[l].in_at(P_->bag_gens[l], t);
      Subr.add_constraint(Min_Up <= 0);
    }
  }
  // 4c
  if (P_->bag_gens[l].size() > 0) {
    for (int t = min_down.getvalue() - 1; t < Num_time; t++) {
      Constraint Min_Down("Min_Down_constraint_" + to_string(t));
      for (int ll = t - min_down.getvalue() + 1; ll < t + 1; ll++) {
        Min_Down += Shut_down[l].in_at(P_->bag_gens[l], ll);
      }
      Min_Down -= 1 - On_off[l].in_at(P_->bag_gens[l], t);
      Subr.add_constraint(Min_Down <= 0);
    }
  }

  ////Ramp Rate
  if (P_->bag_gens[l].size() > 0) {
    Constraint Production_P_LB("Production_P_LB");
    Constraint Production_P_UB("Production_P_UB");
    Constraint Production_Q_LB("Production_Q_LB");
    Constraint Production_Q_UB("Production_Q_UB");
    // 5A
    Production_P_UB = Pg[l] - grid->pg_max * On_off[l];
    Production_P_LB = Pg[l] - grid->pg_min * On_off[l];
    Subr.add_constraint(Production_P_UB.in(P_->bag_gens[l], ind) <= 0);
    Subr.add_constraint(Production_P_LB.in(P_->bag_gens[l], ind) >= 0);

    Production_Q_UB = Qg[l] - grid->qg_max * On_off[l];
    Production_Q_LB = Qg[l] - grid->qg_min * On_off[l];
    Subr.add_constraint(Production_Q_UB.in(P_->bag_gens[l], ind) <= 0);
    Subr.add_constraint(Production_Q_LB.in(P_->bag_gens[l], ind) >= 0);
  }
  // 5C
  if (P_->bag_gens[l].size() > 0) {
    for (int t = 1; t < Num_time; t++) {
      Constraint Ramp_up("Ramp_up_constraint_" + to_string(t));
      Constraint Ramp_down("Ramp_down_constraint_" + to_string(t));
      Ramp_up = Pg[l].in_at(P_->bag_gens[l], t);
      Ramp_up -= Pg[l].in_at(P_->bag_gens[l], t - 1);
      Ramp_up -= rate_ramp * On_off[l].in_at(P_->bag_gens[l], t - 1);
      Ramp_up -= rate_switch * (1 - On_off[l].in_at(P_->bag_gens[l], t));

      Ramp_down = Pg[l].in_at(P_->bag_gens[l], t - 1);
      Ramp_down -= Pg[l].in_at(P_->bag_gens[l], t);
      Ramp_down -= rate_ramp * On_off[l].in_at(P_->bag_gens[l], t);
      Ramp_down -= rate_switch * (1 - On_off[l].in_at(P_->bag_gens[l], t - 1));

      Subr.add_constraint(Ramp_up <= 0);
      Subr.add_constraint(Ramp_down <= 0);
    }

    Constraint Ramp_up("Ramp_up_constraint0" + to_string(l));
    Ramp_up = Pg[l].in_at(P_->bag_gens[l], 0) - Pg_initial.in(P_->bag_gens[l]) -
              rate_ramp.in_at(P_->bag_gens[l], 0) *
                  On_off_initial.in(P_->bag_gens[l]);
    Ramp_up -= rate_switch.in_at(P_->bag_gens[l], 0) *
               (1 - On_off_initial.in(P_->bag_gens[l]));
    Subr.add_constraint(Ramp_up <= 0);

    Constraint Ramp_down("Ramp_down_constraint0");
    Ramp_down =
        -1 * Pg[l].in_at(P_->bag_gens[l], 0) + Pg_initial.in(P_->bag_gens[l]);
    Ramp_down -= rate_ramp.in_at(P_->bag_gens[l], 0) *
                 On_off[l].in_at(P_->bag_gens[l], 0);
    Ramp_down -= rate_switch.in_at(P_->bag_gens[l], 0) *
                 (1 - On_off[l].in_at(P_->bag_gens[l], 0));
    Subr.add_constraint(Ramp_down <= 0);
  }
  //// set the initial state of generators.
  if (P_->bag_gens[l].size() > 0) {
    Constraint gen_initial("gen_initial_" + to_string(l));
    gen_initial += On_off[l].in_at(P_->bag_gens[l], -1) -
                   On_off_initial.in(P_->bag_gens[l]);
    Subr.add_constraint(gen_initial == 0);
  }
  /* solve it! */
  solver solve_Subr(Subr, cplex);
  bool relax = true;
  int output = 1;
  double tol = 1e-6;
  solve_Subr.run(output, relax, tol);
  // LINKED VARIABLES
  // std::string name = Xii[l].in(P_->bag_bus[l],Num_time).get_name();
  //    Xii_log= (*(var<double>*) Subr.get_var(name));
  //    name = R_Xij_.in(P_->bag_bus_pairs_union[l], Num_time).get_name();
  //    R_Xij_log = (*(var<double>*) Subr.get_var(name));
  //    name = Im_Xij.in(P_->bag_bus_pairs_union[l], Num_time).get_name();
  //    Im_Xij_log = (*(var<double>*) Subr.get_var(name));
  return Subr._obj_val;
}

double Global::LR_bound_spatial_() {
  double LB = 0;
  Sub_.clear();
  Sub_.resize(Num_parts);
  for (int l = 0; l < Num_parts; l++) {
    Sub_[l] = Subproblem_spatial_(l);
    LB += Sub_[l];
  }
  return LB;
}

vector<int> Global::check_rank1_constraint_(Model& Sub, int t) {
  auto soc = Sub.get_constraint("SOC_" + to_string(t));
  vector<int> violations;
  string name;
  // for (int p = 0; p < grid->get_bus_pairs().size(); p++) {
  for (int p = 0; p < grid->get_bus_pairs_chord().size(); p++) {
    double eval = soc->eval(p);
    Debug("soc[" << p << "]=" << eval << endl);
    if (eval < -1e-6) {
      violations.push_back(p);
    }
  }
  for (auto& p : grid->get_bus_pairs()) {
    name = p->_name + "," + to_string(t);
    Debug(Im_Xij[t](name).eval() << endl);
    Im_Xij_sol_(name) = Im_Xij[t](name).eval();
    R_Xij_sol_(name) = R_Xij[t](name).eval();
  }

  for (auto& n : grid->nodes) {
    name = n->_name + "," + to_string(t);
    Xii_sol_(name) = Xii[t](name).eval();
  }

  cout << "\# equation violation: " << violations.size() << endl;
  return violations;
}
void Global::add_BenNem_SOCP_time(Model& model, int t, int k) {
  // recursive decomposition to dimension 2 Lorenz cone.
  // we have L^3 rotated SOCP.
  indices ind = indices(to_string(t));
  const auto bus_pairs = grid->get_bus_pairs();
  var<double> gamma("gamma");
  var<double> z("z");
  model.add_var(z.in_at(bus_pairs, t));
  model.add_var(gamma.in_at(bus_pairs, t));

  Constraint Extend_SOCP("Extend_SOCP_1_");
  Extend_SOCP = gamma - 0.5 * Xii[t].from() - 0.5 * Xii[t].to();
  model.add_constraint(Extend_SOCP.in(bus_pairs, ind) == 0);

  Constraint Extend_SOCP2("Extend_SOCP_2_");
  Extend_SOCP2 = 0.5 * Xii[t].from() - 0.5 * Xii[t].to() - z;
  model.add_constraint(Extend_SOCP2.in(bus_pairs, ind) == 0);
  // cone decomposition.
  // R_xij^2 + Im_xij^2 + z^2 <= gamma^2...

  var<double> y("y");  // auxilary vars for cone decomposition
  model.add_var(y.in_at(bus_pairs, t));
  // R_xij^2 + Im_xij^2 <= y^2.
  // y^2 + z^2 < = r^2.
  // BenNem_Lorenz_(model, y(name), z(name), gamma, k);
  vector<var<double>> alpha;
  vector<var<double>> beta;
  for (int i = 1; i < k + 1; i++) {
    var<double> alphai("alpha_" + to_string(i));
    var<double> betai("beta_" + to_string(i));
    model.add_var(alphai.in_at(bus_pairs, t));
    model.add_var(betai.in_at(bus_pairs, t));
    alpha.push_back(alphai);
    beta.push_back(betai);
  }
  for (int i = 0; i < k; i++) {
    Constraint Extend1("Extend1_" + to_string(t) + "_" + to_string(i));
    Constraint Extend2("Extend2_" + to_string(t) + "_" + to_string(i));
    Constraint Extend3("Extend3_" + to_string(t) + "_" + to_string(i));
    if (i == 0) {
      Extend1 = alpha[i] + R_Xij[t];
      Extend2 = -1 * Im_Xij[t] - beta[i];
      Extend3 = Im_Xij[t] - beta[i];
    } else {
      Extend1 = alpha[i] - alpha[i - 1] * cos(M_PI * pow(0.5, i)) -
                beta[i - 1] * sin(M_PI * pow(0.5, i));
      Extend2 = beta[i - 1] * cos(M_PI * pow(0.5, i)) -
                alpha[i - 1] * sin(M_PI * pow(0.5, i)) - beta[i];
      Extend3 = alpha[i - 1] * sin(M_PI * pow(0.5, i)) -
                beta[i - 1] * cos(M_PI * pow(0.5, i)) - beta[i];
    }
    // Extend3.print_expanded();
    model.add_constraint(Extend1.in(bus_pairs, ind) == 0);
    model.add_constraint(Extend2.in(bus_pairs, ind) <= 0);
    model.add_constraint(Extend3.in(bus_pairs, ind) <= 0);
  }
  Constraint Extend4("Extend4_" + to_string(t));
  Extend4 = y - alpha[k - 1] * cos(M_PI * pow(0.5, k)) -
            beta[k - 1] * sin(M_PI * pow(0.5, k));
  model.add_constraint(Extend4.in(bus_pairs, ind) == 0);

  vector<var<double>> alphay;
  vector<var<double>> betaz;
  for (int i = 1; i < k + 1; i++) {
    var<double> alphayi("alphay_" + to_string(i));
    var<double> betazi("betaz_" + to_string(i));
    model.add_var(alphayi.in_at(bus_pairs, t));
    model.add_var(betazi.in_at(bus_pairs, t));
    alphay.push_back(alphayi);
    betaz.push_back(betazi);
  }
  for (int i = 0; i < k; i++) {
    Constraint Extend1("Extend1_yz" + to_string(t) + "_" + to_string(i));
    Constraint Extend2("Extend2_yz" + to_string(t) + "_" + to_string(i));
    Constraint Extend3("Extend3_yz" + to_string(t) + "_" + to_string(i));
    if (i == 0) {
      Extend1 = alphay[i] + y;
      Extend2 = -1 * z - betaz[i];
      Extend3 = z - betaz[i];
    } else {
      Extend1 = alphay[i] - alphay[i - 1] * cos(M_PI * pow(0.5, i)) -
                betaz[i - 1] * sin(M_PI * pow(0.5, i));
      Extend2 = betaz[i - 1] * cos(M_PI * pow(0.5, i)) -
                alphay[i - 1] * sin(M_PI * pow(0.5, i)) - betaz[i];
      Extend3 = alphay[i - 1] * sin(M_PI * pow(0.5, i)) -
                betaz[i - 1] * cos(M_PI * pow(0.5, i)) - betaz[i];
    }
    model.add_constraint(Extend1.in(bus_pairs, ind) == 0);
    model.add_constraint(Extend2.in(bus_pairs, ind) <= 0);
    model.add_constraint(Extend3.in(bus_pairs, ind) <= 0);
  }
  Constraint Extend4yz("Extend4_yz" + to_string(t));
  Extend4yz = gamma - alphay[k - 1] * cos(M_PI * pow(0.5, k)) -
              betaz[k - 1] * sin(M_PI * pow(0.5, k));
  model.add_constraint(Extend4yz.in(bus_pairs, ind) == 0);
}

void Global::add_SDP_S_(Model& model, vector<int> indices, int t) {
  // find all bags containning this link.
  auto V = check_rank1_constraint_(model, t);
  string name;
  if (V.size() > 0) {
    // find eigenvectors..with negative values...
    arma::SpMat<double> val(2 * grid->nodes.size(), 2 * grid->nodes.size());
    for (auto& node : grid->nodes) {
      name = node->_name + "," + to_string(t);
      val(2 * node->_id, 2 * node->_id) = Xii_sol_(name).eval();
      val(2 * node->_id + 1, 2 * node->_id + 1) = Xii_sol_(name).eval();
      val(2 * node->_id, 2 * node->_id + 1) = 0;
      val(2 * node->_id + 1, 2 * node->_id) = 0;
    }
    // for (auto &pair: grid->get_bus_pairs()) {
    //  name = pair->_name + "," + to_string(t);
    //            val(2*pair->_src->_id, 2*pair->_dest->_id) = R_Xij_sol_(name);
    //            val(2*pair->_src->_id+1, 2*pair->_dest->_id+1) =
    //            R_Xij_sol_(name); val(2*pair->_src->_id, 2*pair->_dest->_id+1)
    //            =  -Im_Xij_sol_(name); val(2*pair->_src->_id+1,
    //            2*pair->_dest->_id* =  Im_Xij_sol_(name);
    //}
  }
}

vector<vector<string>> Global::get_3dmatrix_index(
    Net* net, const vector<vector<Node*>> bags, int t) {
  string key;
  vector<vector<string>> res;
  int size = 3;
  res.resize(size);  // 3d cuts
  set<vector<unsigned>> ids;
  for (auto& bag : bags) {
    if (bag.size() != size) {
      continue;
    }
    vector<unsigned> ids_bag;  // avoid redundant bags.
    for (int i = 0; i < size; i++) {
      ids_bag.push_back(bag[i]->_id);
    }
    if (ids.count(ids_bag) == 0) {
      ids.insert(ids_bag);
    } else {
      continue;
    }
    for (int i = 0; i < size - 1; i++) {
      if (net->get_directed_arc(bag[i]->_name, bag[i + 1]->_name) != nullptr) {
        key = bag[i]->_name + "," + bag[i + 1]->_name + "," + to_string(t);
      } else {
        throw invalid_argument("index should obey i < j!!");
        key = bag[i + 1]->_name + "," + bag[i]->_name + "," + to_string(t);
      }
      res[i].push_back(key);
    }
    /* Loop back pair */
    if (net->get_directed_arc(bag[0]->_name, bag[size - 1]->_name) != nullptr) {
      key = bag[0]->_name + "," + bag[size - 1]->_name + "," + to_string(t);
    } else {
      throw invalid_argument("index should obey i < j!!");
      key = bag[size - 1]->_name + "," + bag[0]->_name + "," + to_string(t);
    }
    res[size - 1].push_back(key);
  }
  return res;
}
std::vector<std::vector<string>> Global::get_3ddiagon_index(
    const std::vector<std::vector<Node*>> bags, int t) {
  string key;
  vector<vector<string>> res;
  int size = 3;
  res.resize(size);  // 3d cuts
  for (auto& bag : bags) {
    if (bag.size() != size) {
      continue;
    }
    for (int i = 0; i < size; i++) {
      key = bag[i]->_name + "," + to_string(t);
      res[i].push_back(key);
    }
  }
  return res;
}

// void Global::add_3d_cuts_(Model& model, vector<int> indices, int t){
// void Global::add_3d_cuts_static(Model& model, int t) {
//    auto keys = get_3dmatrix_index(chordal, grid->_bags, t);
//    auto keyii = get_3ddiagon_index(grid->_bags, t);
//    vector<var<double>> R_Xij_;
//    vector<var<double>> Im_Xij_;
//    vector<var<double>> Xii_;
//    R_Xij_.resize(3);
//    Im_Xij_.resize(3);
//    Xii_.resize(3);
//    for (int i = 0; i < 3; i++){
//        R_Xij_[i] = R_Xij[t].in(keys[i]);
//        R_Xij_[i]._name += to_string(i);
//        Im_Xij_[i] = Im_Xij[t].in(keys[i]);
//        Im_Xij_[i]._name += to_string(i);
//        Xii_[i] = Xii[t].in(keyii[i]);
//        Xii_[i]._name += to_string(i);
//        Xii_[i]._unique_id =
//        make_tuple<>(Xii[t].get_id(),in_,typeid(double).hash_code(), 0, i);
//        R_Xij_[i]._unique_id =
//        make_tuple<>(R_Xij[t].get_id(),in_,typeid(double).hash_code(), 0, i);
//        Im_Xij_[i]._unique_id =
//        make_tuple<>(Im_Xij[t].get_id(),in_,typeid(double).hash_code(), 0, i);
//    }
//    Constraint sdpcut("3dcuts_");
//    sdpcut = 2.0*R_Xij_[0]*(R_Xij_[1]*R_Xij_[2] +Im_Xij_[1]*Im_Xij_[2]);
//    sdpcut += 2.0*Im_Xij_[0]*(R_Xij_[1]*Im_Xij_[2] -Im_Xij_[1]*R_Xij_[2]);
//    sdpcut -= (power(R_Xij_[0], 2) + power(Im_Xij_[0], 2)) * Xii_[2];
//    sdpcut -= (power(R_Xij_[1], 2) + power(Im_Xij_[1], 2)) * Xii_[0];
//    sdpcut -= (power(R_Xij_[2], 2) + power(Im_Xij_[2], 2)) * Xii_[1];
//    sdpcut += Xii_[0]*Xii_[1]*Xii_[2];
//    DebugOn("\nsdpcut nb inst = " << sdpcut.get_nb_instances() << endl);
//    model.add_constraint(sdpcut >= 0);
//}

void Global::add_3d_cuts_static(Model& model, int t) {
  //    if(grid->add_3d_nlin) {
  //        DebugOn("Adding 3d determinant polynomial cuts\n");
  //        auto R_Wij_ = R_Xij[t].pairs_in_directed(*grid, grid->_bags, 3, t);
  //        auto Im_Wij_ = Im_Xij[t].pairs_in_directed(*grid, grid->_bags, 3,
  //        t); auto Wii_ = Xii[t].in_at(grid->_bags, 3, t); auto I_sgn =
  //        signs(*grid, grid->_bags, t); Constraint SDP3("SDP_3D"); SDP3 =
  //        2*R_Wij_[0]*(R_Wij_[1] * R_Wij_[2] + I_sgn[1] * I_sgn[2] *
  //        Im_Wij_[1] * Im_Wij_[2]); SDP3 += 2*I_sgn[0]*Im_Wij_[0]*(R_Wij_[1] *
  //        I_sgn[2] * Im_Wij_[2] - I_sgn[1] * Im_Wij_[1] * R_Wij_[2]); SDP3 -=
  //        (power(R_Wij_[0],2) + power(Im_Wij_[0], 2))*Wii_[2]; SDP3 -=
  //        (power(R_Wij_[1],2) + power(Im_Wij_[1], 2))*Wii_[0]; SDP3 -=
  //        (power(R_Wij_[2],2) + power(Im_Wij_[2], 2))*Wii_[1]; SDP3 +=
  //        Wii_[0]*Wii_[1]*Wii_[2]; DebugOn("\nsdp nb inst = " <<
  //        SDP3.get_nb_instances() << endl); Sdpcut_outer_[t] =
  //        model.add_constraint(SDP3 >= 0);
  //    }
}

vector<param<>> Global::signs(Net& net,
                              const std::vector<std::vector<Node*>>& bags,
                              int t) {
  vector<param<>> res;
  string key;
  size_t idx;
  res.resize(3);
  for (int i = 0; i < 3; i++) {
    res[i].set_name("I_sign_" + to_string(t) + "_" + to_string(i));
  }
  set<vector<unsigned>> ids;
  for (auto& bag : net._bags) {
    if (bag.size() != 3) {
      continue;
    }
    vector<unsigned> ids_bag;
    for (int i = 0; i < 3; i++) {
      ids_bag.push_back(bag[i]->_id);
    }
    if (ids.count(ids_bag) == 0) {
      ids.insert(ids_bag);
    } else {
      continue;
    }
    for (int i = 0; i < 2; i++) {
      if (net.has_directed_arc(bag[i], bag[i + 1])) {
        key = bag[i]->_name + "," + bag[i + 1]->_name + "," + to_string(t);
        idx = res[i].set_val(key, 1.0);
        res[i]._ids->at(0).push_back(idx);
      } else {
        key = bag[i + 1]->_name + "," + bag[i]->_name + "," + to_string(t);
        DebugOff("\nreversed arc " << key);
        idx = res[i].set_val(key, -1.0);
        res[i]._ids->at(0).push_back(idx);
      }
    }
    /* Loop back pair */
    if (net.has_directed_arc(bag[0], bag[2])) {
      key = bag[0]->_name + "," + bag[2]->_name + "," + to_string(t);
      idx = res[2].set_val(key, 1.0);
      res[2]._ids->at(0).push_back(idx);
    } else {
      key = bag[2]->_name + "," + bag[0]->_name + "," + to_string(t);
      DebugOff("\nreversed arc " << key);
      idx = res[2].set_val(key, -1.0);
      res[2]._ids->at(0).push_back(idx);
    }
  }
  for (int i = 0; i < 3; i++) {
    res[i]._dim[0] = res[i]._ids->at(0).size();
    res[i]._is_indexed = true;
  }
  return res;
}
