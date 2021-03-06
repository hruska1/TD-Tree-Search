#include "Tests_TDUCT.hpp"

namespace Tests_TDUCT
{

	//Improved LRP search v1 - restart from kNN best average score after a batch of iterations
	//kNN is weighted according to the number of repeated evaluations (num_repeats), evaluated sample can be set to a higher weight
	void LRP_improved_v1(double* score_avg, double* score_avg_last10, double** last_param_val, bool force_setting_output, const int set_final_evaluations, double* avg_num_games, double* final_eval_score)
	{


		//konfiguracija tom
		const int		num_iterations_self = 10;						//number MCTS simulations per move: evaluated player
		const int		num_iterations_opponent = 10;	//number MCTS simulation per move: opponent
		const int		num_games_start = 10;				//number of games per score output at LRP start
		const int		num_games_end = num_games_start;	//number of games per score output at LRP end
		const double	min_increase_num_games_fact = 1.0;		//minimal increase factor of games per evaluation if confidence below threshold (is ignored if max_increase_num_games_fact <= 1.0)
		const double	max_increase_num_games_fact = 1.0;		//maximal increase factor of games per evaluation requested by LRP confidence statistical test (disable by setting <= 1.0)
		const double	start_Cp_self = 0.2;		//initial Cp: evaluated player
		const double	start_RAVE_V_self = 35.0;		//initial RAVE parameter V: evaluated player
		const double	fixed_CP_opponent = 0.2;		//initial Cp: opponent
		const double	funcApp_init_weights = 0.0;		//initial function approximator weights (to be optimized by LRP)
		const int		num_LRP_iterations = 2000;		//number of LRP iterations (must be > 1 otherwise invalid values appear)
		const double	dw_start = 0.2000;	//LRP delta weight (change in Cp value): at start
		const double	dw_limit = 0.0010;	//LRP delta weight (change in Cp value): at end
		const double	lrp_ab_max = 0.75;		//LRP learning parameter alpha: maximal value
		const double	lrp_ab_min = 0.02;		//LRP learning parameter alpha: minmal value
		const double	lrp_ab_dscore_min = 1.0 / (double)((num_games_start + num_games_end) / 2.0);	//LRP minimum delta score (to change probabilites with minimum alpha value)
		const double	lrp_ab_dscore_max = lrp_ab_dscore_min * 10.0;	//MUST BE HIGHER OR EQUAL TO lrp_ab_dscore_min
		const double	lrp_b_koef = 1;		//LRP learning parameter beta koeficient [0,1] of alpha value
		const bool		exp_dw_decrease = 0;		//use exponential weight decrease (instead of linear)
		const int		eval_player_position = 1;		//which player to optimize and evaluate 0 or 1, currently works only for two-player games
		const bool		LRP_output_best_kNN = true;					//if true final param values will be set according to kNN best avg, otherwise final param values are left from the last LRP iteration (true is default setting)
		const bool		KNN_weight_repeats = true;					//LRP best neighbour by kNN weight by number of repeats per sample (true is default setting)
		const double	LRP_best_neighbour_weight_central = 1.1;	//scalar factor of central (observed) sample weight when averaging with kNN	(1.1 is default setting, 1.0 means equal weight to other samples)

		//number of games to evaluate final combination of parameters
		//const int		final_evaluation_num_games = 5000;
		const int		final_evaluation_num_games = set_final_evaluations;
		//20000, 95% confidence that true value deviates less by 1%
		//5000,  95% confidence that true value deviates less by 2%
		//1200,  95% confidence that true value deviates less by 4%
		//750,   95% confidence that true value deviates less by 5% (default setting)
		//200,	 95% confidence that true value deviates less by 10%
		const int		final_evaluation_num_iterations_self = num_iterations_self;
		const int		final_evaluation_num_iterations_opponent = num_iterations_opponent;
		const int		final_eval_player_position = eval_player_position;

		//number of LRP_iterations between restarts from best position in parameter space (disable by setting to num_LRP_iterations or 0)	
		//const int		LRP_restart_condition_iterations = 0;	//never restart
		const int		LRP_restart_condition_iterations = (int)(sqrt(num_LRP_iterations) + 0.5);	//sqare root of LRP iterations (default setting)
		//const int		LRP_restart_condition_iterations = (int)(num_LRP_iterations/10.0 + 0.5);	//linear to numer of LRP iterations

		//user-defined equations for LRP_best_neighbours_num: number of nearest neighbours when searching combination of parameters with best average score
		const int		LRP_best_neighbours_num = (int)(min((sqrt(num_LRP_iterations / 10.0 * 1000.0 / ((num_games_start + num_games_end) / 2.0)))*0.5, num_LRP_iterations*0.1*LRP_best_neighbour_weight_central) + 0.5);	//sqare root of num LRP iterations and games (default setting)
		//const int		LRP_best_neighbours_num = (int)(min( (sqrt(num_LRP_iterations/20.0 * 1000.0/((num_games_start+num_games_end)/2.0)))*0.5 , num_LRP_iterations*0.1*LRP_best_neighbour_weight_central) + 0.5);	//less KNN, sqare root of num LRP iterations and games
		//const int		LRP_best_neighbours_num = (int)(min( (sqrt(num_LRP_iterations/10.0)*(100.0/((num_games_start+num_games_end)/2.0))) , num_LRP_iterations*0.1*0.5*LRP_best_neighbour_weight_central) + 0.5);	//sqare root of num LRP iterations, linear in games	

		//restart from best only if current score differs from best by more than this factor, if (1-current/best >= factor) then restart) (0.0 restarts always, also if last is best), is ignored if LRP_restart_condition_iterations == 0
		//const double	LRP_restart_condition_score_rel_diff = 0.0;	//always restart
		const double	LRP_restart_condition_score_rel_diff = 0.1 * 1 / sqrt(LRP_best_neighbours_num) * LRP_best_neighbour_weight_central;	//(default setting)
		const double	LRP_restart_condition_score_rel_diff_end = 0.2 * LRP_restart_condition_score_rel_diff;	//the factor decreases linearly by the number of iterations down to this value

		//confidence interval factor for assessing if two scores belong to different distributions
		const bool		LRP_change_AB_by_confidence = true;		//change LRP alpha-beta parameters proportionally to confidence between scores (true, default setting) or to absolute difference (false)
		const bool		LRP_change_DW_by_confidence = false;	//NOT YET IMPLEMENTED: change LRP delta weight proportionally to confidence between scores (true) or linearly/exponentially between dw_start and dw_limit (false, default setting)
		const double	lrp_score_diff_conf_factor = 1.44;		//(1.44 <- 85% is default setting)
		//99% -> 2.58
		//95% -> 1.96
		//90% -> 1.645
		//85% -> 1.44
		//80% -> 1.28
		//75% -> 1.15
		//70% -> 1.04

		//THE NUMBER OF Func_App_UCT_Params must be set in Player_AI_UCT_TomTest.hpp with the constant TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_PARAMS


		//TODO1: da ce je slab�i rezultat, da razveljavi� in da� na prej�nje stanje, parameter pa ni treba dat celo pot nazaj ampak samo do dolo�ene mere

		//TODO2 (ve� dela): preizkusi �e bi dva lo�ena LRPja delovala bolj�e za iskanje 2 parametrov, 2 varianti:
		//			*oba LRPja isto�asno izvedeta akcijo in opazujeta izhod
		//			*LRPja izmeni�no izvajata akcije (to pomeni, da je na razpolago pol manj korakov za posamezen LRP)

		const bool show_full_output = true; //runtime setting

		//Game_Engine* game = new Game_ConnectFour();
		//Game_Engine* game = new Game_Gomoku();
		Game_Engine* game = new Game_TicTacToe();
		//Game_Engine* game = new Game_Hex();

		Player_AI_UCT_TomTest* optimizingPlayer = new Player_AI_UCT_TomTest();
		Player_AI_UCT* opponent = new Player_AI_UCT(game);

		Tom_Function_Approximator* funcApp1 = new Tom_Function_Approximator();
		funcApp1->Initialize();
		funcApp1->num_results = optimizingPlayer->Func_App_UCT_num_params;
		funcApp1->Settings_Apply_New();

		for (int i = 0; i < funcApp1->num_params; i++)
			funcApp1->parameter_weights[i] = funcApp_init_weights;

		optimizingPlayer->Func_App_UCT_Params = funcApp1;
		optimizingPlayer->game = game;
		optimizingPlayer->Initialize();
		optimizingPlayer->Output_Log_Level_Create_Headers();

		optimizingPlayer->UCT_param_C = start_Cp_self;
		optimizingPlayer->RAVE_param_V = start_RAVE_V_self;

		////2014_01 Q-Values test: best 11 param LRP search settings ... and ...  _2014_02_12 Gomo SARSA+RAVE search 7 par
		////setting TOMPLAYER_AI_UCT_TOMTEST_QVALUE_TRACE_GAMMA_TV = 1
		//GOMOKU
		//optimizingPlayer->UCT_param_C = 0.079;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.395;
		//optimizingPlayer->QVALUE_param_alpha = 1.229;
		//optimizingPlayer->QVALUE_param_gamma = 1.739;
		//optimizingPlayer->QVALUE_param_lambda = 0.125;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.843;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0.132;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 2.023;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = -1.266;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1.540;
		//optimizingPlayer->QVALUE_param_initVal = 0.407;

		////2014_02 Q-Values test fixed leaf node val
		//optimizingPlayer->UCT_param_C = 0.08;

		//2014_02_24 Conn4 SARSA+AMAF search 3 par
		//connect 4
		//optimizingPlayer->UCT_param_C = 0.122;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.141;
		//optimizingPlayer->QVALUE_param_alpha = 0.744;
		//optimizingPlayer->QVALUE_param_gamma = 1.229;
		//optimizingPlayer->QVALUE_param_lambda = 0.466;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.815;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 2.136;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1.099;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = -0.561;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 3.875;
		//optimizingPlayer->QVALUE_param_initVal = 1.366;

		//-- 2014_03_01 Gomo SARSA+AMAF_EQ3 dimnish params
		//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 3, set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 2, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM = 3
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//	//search 7
		//optimizingPlayer->UCT_param_C = -5.3042;		
		//optimizingPlayer->AMAF_param_alpha = 0.9169;	
		//optimizingPlayer->QVALUE_param_scoreWeight = 2.9037;	
		//optimizingPlayer->QVALUE_param_alpha = 1.229;
		//optimizingPlayer->QVALUE_param_gamma = 1.739;
		//optimizingPlayer->QVALUE_param_lambda = 0.125;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.843;
		//	//search 6 - lambda_playOut=1
		////optimizingPlayer->QVALUE_param_lambda_playOut = 1.0;	//2014_03_01 Gomo SARSA+AMAF_EQ3 dimnish params - search 6
		//	//search 3
		////optimizingPlayer->QVALUE_param_alpha = 0.2;
		////optimizingPlayer->QVALUE_param_gamma = 0.99;
		////optimizingPlayer->QVALUE_param_lambda = 0.25;
		//-- end - 2014_03_01 Gomo SARSA+AMAF_EQ3 dimnish params

		//-- 2014_03_01 Conn4 SARSA+AMAF_EQ3 dimnish params
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//	//search 7
		//optimizingPlayer->UCT_param_C = 0.150;		
		//optimizingPlayer->AMAF_param_alpha = -0.792;	
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.120;	
		//optimizingPlayer->QVALUE_param_alpha = 0.744;
		//optimizingPlayer->QVALUE_param_gamma = 1.229;
		//optimizingPlayer->QVALUE_param_lambda = 0.466;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.815;
		//-- end - 2014_03_01 Conn4 SARSA+AMAF_EQ3 dimnish params

		//-- 2014_03_03 SARSA+AMAF_EQ3 direct search 12 par - configuration
		//-- 2014_03_07 SARSA+AMAF_EQ3 single NN by sum_iter - configuration
		//	//gomoku
		//optimizingPlayer->UCT_param_C = -5.7522;		
		//optimizingPlayer->AMAF_param_alpha = 0.8997;	
		//optimizingPlayer->QVALUE_param_scoreWeight = 3.5116;	
		//optimizingPlayer->QVALUE_param_alpha = 0.3124;
		//optimizingPlayer->QVALUE_param_gamma = 1.0004;
		//optimizingPlayer->QVALUE_param_lambda = 0.6664;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.9129;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//	//connect4
		//optimizingPlayer->UCT_param_C = 0.1747;		
		//optimizingPlayer->AMAF_param_alpha = -0.7338;	
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.3131;	
		//optimizingPlayer->QVALUE_param_alpha = 0.7833;
		//optimizingPlayer->QVALUE_param_gamma = 1.0839;
		//optimizingPlayer->QVALUE_param_lambda = 0.397;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.8754;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		//-- end - 2014_03_03 SARSA+AMAF_EQ3 direct  search 12 par - configuration

		////-- 2014_03_05 SARSA+AMAF_EQ3 TicTacToe search 7 par - configuration
		////-- 2014_03_06 MCTSiter op_X self_X direct search par
		//optimizingPlayer->UCT_param_C = 0;		
		//optimizingPlayer->AMAF_param_alpha = 0;	
		//optimizingPlayer->RAVE_param_V = 0;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0;	
		//optimizingPlayer->QVALUE_param_alpha = 0;
		//optimizingPlayer->QVALUE_param_gamma = 0;
		//optimizingPlayer->QVALUE_param_lambda = 0;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////-- 2014_03_06 MCTSiter op_X self_X gomo SARSA+AMAF_EQ3 RUN3 direct search par 3 and 3-6
		//optimizingPlayer->UCT_param_C = 0.2859;		
		//optimizingPlayer->AMAF_param_alpha = -1.0040;	
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.8373;	
		//optimizingPlayer->QVALUE_param_alpha = 0.3124;
		//optimizingPlayer->QVALUE_param_gamma = 1.0004;
		//optimizingPlayer->QVALUE_param_lambda = 0.6664;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.9129;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		//-- 2014_02_06 SARSA (without AMAF) and fixed leaf and init - search 5 par
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//optimizingPlayer->UCT_param_C = 0.08;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.6203;
		//optimizingPlayer->QVALUE_param_alpha = 0.2954;
		//optimizingPlayer->QVALUE_param_gamma = 2.3039;
		//optimizingPlayer->QVALUE_param_lambda = 0.3282;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.899;

		//-- 2014_03_07 SARSA+AMAF_EQ3 single NN by sum_iter Gomo iter1000-1000 RUN2
		//funcApp1->parameter_weights[0] = -2.8608;
		//funcApp1->parameter_weights[1] = -0.4007;
		//funcApp1->parameter_weights[2] = 0.1647;
		//funcApp1->parameter_weights[3] = -0.9058;
		//funcApp1->parameter_weights[4] = 1.3514;
		//funcApp1->parameter_weights[5] = 0.9685;
		//funcApp1->parameter_weights[6] = -0.7590;
		//funcApp1->parameter_weights[7] = -0.7765;
		//funcApp1->parameter_weights[8] = -0.0926;

		//-- 2014_03_17 paper table1

		////SARSA1+RAVE
		//optimizingPlayer->UCT_param_C = 0.0544;		
		//optimizingPlayer->RAVE_param_V = 35.0;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.7036;	
		//optimizingPlayer->QVALUE_param_alpha = 0.7209;
		//optimizingPlayer->QVALUE_param_gamma = 0.9072;
		//optimizingPlayer->QVALUE_param_lambda = 0.6071;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85812;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA2+RAVE
		//optimizingPlayer->UCT_param_C = -2.0;
		//optimizingPlayer->RAVE_param_V = 35.0;
		//optimizingPlayer->QVALUE_param_scoreWeight = 2.0;
		//optimizingPlayer->QVALUE_param_alpha = 0.3124;
		//optimizingPlayer->QVALUE_param_gamma = 1.0004;
		//optimizingPlayer->QVALUE_param_lambda = 0.6664;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.9129;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//funcApp1->parameter_weights[0] = 0.0;
		//funcApp1->parameter_weights[1] = 1.5;
		//funcApp1->parameter_weights[2] = 3;

		////UCT+AMAF_EQ3
		//optimizingPlayer->UCT_param_C = 0.10;
		//optimizingPlayer->AMAF_param_alpha = 0.80;

		////SARSA gamma 0
		////set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_TRACE_GAMMA_TV		0
		//optimizingPlayer->UCT_param_C = 0.0544;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.7036;	
		//optimizingPlayer->QVALUE_param_alpha = 0.7209;
		//optimizingPlayer->QVALUE_param_gamma = 0.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.55;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85812;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT gamma 1 (and QleafVal)
		////set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_TRACE_GAMMA_TV		1
		//optimizingPlayer->UCT_param_C = 0.04234;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;	
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 1.0;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.7329;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT gamma 1 run2 (and variants)
		//optimizingPlayer->UCT_param_C = 0.0227;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;	
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.6206;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85856;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//funcApp1->parameter_weights[0] = 0.1761;
		//funcApp1->parameter_weights[1] = -0.0136;
		//funcApp1->parameter_weights[2] = 0.1234;

		////SARSA-CORRECT + AMAF_EQ3
		//optimizingPlayer->UCT_param_C = -2.0;
		//optimizingPlayer->AMAF_param_alpha = 0.8997;
		//optimizingPlayer->QVALUE_param_scoreWeight = 2.0;	
		////optimizingPlayer->QVALUE_param_alpha = 1.0;		//search 5 params
		//optimizingPlayer->QVALUE_param_alpha = 0.3124;	//search 6 params
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.6206;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85856;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run3 (and same-lambda)
		//optimizingPlayer->UCT_param_C = 0.025;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;	
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.62;
		////optimizingPlayer->QVALUE_param_lambda = 0.90;		//version: SAME LAMBDA
		////optimizingPlayer->QVALUE_param_lambda = 1.0;		//version: LAM=1
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.90;
		////optimizingPlayer->QVALUE_param_lambda_playOut = 1.00;	//version: LAMply=1
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run4 - individual parameters
		//optimizingPlayer->UCT_param_C = 0.03584;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;	
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.88836;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.88836;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run5 - individual parameters
		//optimizingPlayer->UCT_param_C = 0.04944;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.7943;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.8853;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.8853;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		////funcApp1->parameter_weights[0] = -0.1694;
		////funcApp1->parameter_weights[1] = -0.0433;
		////funcApp1->parameter_weights[3] = -0.9036;
		////funcApp1->parameter_weights[4] = -0.9036;

		////SARSA-CORRECT run5-1 - variant1
		//optimizingPlayer->UCT_param_C = 0.01556;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.78564;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.70458;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.70458;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		//////SARSA-CORRECT run5+AMAF_EQ3 - individual parameters
		//optimizingPlayer->UCT_param_C = -2.0;
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.5;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.8853;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.8853;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//optimizingPlayer->AMAF_param_alpha = 0.80;
		//optimizingPlayer->RAVE_param_V = 35.0;
		//funcApp1->parameter_weights[0] = -4.0;

		//SARSA-CORRECT run5 - individual parameters
		//optimizingPlayer->UCT_param_C = 0.04944;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.7943;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.8853;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.8853;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		////funcApp1->parameter_weights[0] = -2.0;
		////funcApp1->parameter_weights[1] = 0.25;
		////optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run6
		//optimizingPlayer->UCT_param_C = 0.04944;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.7943;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.62392;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.8764;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run7+AMAF3 (or RAVE)
		//optimizingPlayer->UCT_param_C = -5.0;
		//optimizingPlayer->QVALUE_param_scoreWeight = 2.0;
		//optimizingPlayer->QVALUE_param_alpha = 0.5;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.58;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//optimizingPlayer->AMAF_param_alpha = 0.90;
		//optimizingPlayer->RAVE_param_V = 35.0;
		////optimizingPlayer->QVALUE_param_initVal = 0.0;	//RUN 7-1 translated0.5
		////funcApp1->parameter_weights[0] = 0.5;
		////funcApp1->parameter_weights[1] = 0.15;
		////funcApp1->parameter_weights[2] = 0.5;
		////funcApp1->parameter_weights[3] = -0.25;

		////TDfourth run1 QleafVal
		//optimizingPlayer->UCT_param_C = 0.0191;
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 0.854;
		//optimizingPlayer->QVALUE_param_lambda = 1.0;
		////optimizingPlayer->QVALUE_param_lambda = 0.584;
		//optimizingPlayer->QVALUE_param_initVal = 0.0;
		//funcApp1->parameter_weights[0] = 0.15;
		//funcApp1->parameter_weights[1] = 0.06;

		//////TDfourth - TDmin (WR) + AMAF3 (2014_04_28 paper T1 TDfourth TD-UCT WR + aAMAF2)
		////double TD_leaf_state_value = currentNode->Q_value;;
		////MC_value = set_QVALUE_param_scoreWeight*(child->Q_value+0.5) + set_MC_weight*(MC_value);	//TDfourth
		//optimizingPlayer->UCT_param_C = 0.052;
		//optimizingPlayer->AMAF_param_alpha = 0.90;
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;
		//optimizingPlayer->QVALUE_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_gamma = 0.877;
		//optimizingPlayer->QVALUE_param_lambda = 1.0;
		//optimizingPlayer->QVALUE_param_initVal = 0.0;

		//-- 2014_03_11 paper optimizations F3, F4 and T2 (different games)

		//////UCB, RAVE
		//optimizingPlayer->UCT_param_C = 0.0;
		//optimizingPlayer->RAVE_param_V = 0.0;
		//funcApp1->parameter_weights[0] = -0.16;
		//funcApp1->parameter_weights[1] = 0.13;

		//////SARSA
		//optimizingPlayer->UCT_param_C = 0.0544;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.7036;	
		//optimizingPlayer->QVALUE_param_alpha = 0.7209;
		//optimizingPlayer->QVALUE_param_gamma = 0.9072;
		//optimizingPlayer->QVALUE_param_lambda = 0.6071;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85812;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//funcApp1->parameter_weights[0] = 0.07;
		//funcApp1->parameter_weights[1] = 0.25;
		//funcApp1->parameter_weights[2] = 0.23;
		//funcApp1->parameter_weights[3] = 0.35;
		//funcApp1->parameter_weights[4] = -0.1;

		////////SARSA gomoku19x19 8000-2000 (run2)
		//optimizingPlayer->UCT_param_C = 0.7043;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.9658;	
		//optimizingPlayer->QVALUE_param_alpha = 0.7209;
		//optimizingPlayer->QVALUE_param_gamma = 0.9072;
		//optimizingPlayer->QVALUE_param_lambda = 0.6071;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85812;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		////funcApp1->parameter_weights[0] = 0.07;
		////funcApp1->parameter_weights[1] = 0.25;
		////funcApp1->parameter_weights[2] = 0.23;
		////funcApp1->parameter_weights[3] = 0.35;
		////funcApp1->parameter_weights[4] = -0.1;

		//////SARSA+AMAF
		//optimizingPlayer->UCT_param_C = -2.0;
		//optimizingPlayer->AMAF_param_alpha = 0.8997;
		////optimizingPlayer->AMAF_param_alpha = 1.0;
		//optimizingPlayer->QVALUE_param_scoreWeight = 2.0;
		//optimizingPlayer->QVALUE_param_alpha = 0.3124;
		//optimizingPlayer->QVALUE_param_gamma = 1.000;
		//optimizingPlayer->QVALUE_param_lambda = 0.6664;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.9129;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//funcApp1->parameter_weights[0] = -3.309;
		//funcApp1->parameter_weights[1] = -2;
		//funcApp1->parameter_weights[2] = -1.7102;
		//funcApp1->parameter_weights[3] = 0.0;
		//funcApp1->parameter_weights[4] = 0.0;
		//funcApp1->parameter_weights[5] = 0.0;

		////////SARSA (Hex7x7 run2)
		//optimizingPlayer->UCT_param_C = 0.13;	
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.3242;
		//optimizingPlayer->QVALUE_param_alpha = 0.1563;
		//optimizingPlayer->QVALUE_param_gamma = 1.000;
		//optimizingPlayer->QVALUE_param_lambda = 0.63945;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.91768;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		//////SARSA+AMAF (Hex7x7 run2)
		//optimizingPlayer->UCT_param_C = -2.8378;
		//optimizingPlayer->AMAF_param_alpha = 0.8253;
		//optimizingPlayer->QVALUE_param_scoreWeight = 0.9124;
		//optimizingPlayer->QVALUE_param_alpha = 0.2401;
		//optimizingPlayer->QVALUE_param_gamma = 1.000;
		//optimizingPlayer->QVALUE_param_lambda = 0.7627;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.9465;
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		//-- end- 2014_03_11 paper optimizations F3, F4 and T2 (different games)

		////SARSA-third test1
		//optimizingPlayer->UCT_param_C = 0.05;
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;
		//optimizingPlayer->QVALUE_param_alpha = 0.2;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 1.0;
		//optimizingPlayer->QVALUE_param_initVal = 0.0;

		////  SARSA(lambda) two + AMAF
		//optimizingPlayer->QVALUE_param_leaf_node_val0 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val1 = 1;
		//optimizingPlayer->QVALUE_param_leaf_node_val2 = 0;
		//optimizingPlayer->QVALUE_param_leaf_node_val3 = 1;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;
		//optimizingPlayer->UCT_param_C = -5.75;
		//optimizingPlayer->AMAF_param_alpha = 0.90;
		//optimizingPlayer->QVALUE_param_scoreWeight = 3.51;
		//optimizingPlayer->QVALUE_param_alpha = 0.3124;
		//optimizingPlayer->QVALUE_param_gamma = 0.9129;
		//optimizingPlayer->QVALUE_param_lambda = 0.730;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.9129;

		optimizingPlayer->UCT_param_C = 0.0;
		optimizingPlayer->QVALUE_param_scoreWeight = 0.0;
		optimizingPlayer->QVALUE_param_alpha = 0.0;
		optimizingPlayer->QVALUE_param_gamma = 0.0;
		optimizingPlayer->QVALUE_param_lambda = 0.0;
		optimizingPlayer->QVALUE_param_initVal = 0.0;

		//-------- end of settings -------- //

		bool silence_output = ((score_avg != NULL) || (score_avg_last10 != NULL) || (last_param_val != NULL));

		//apply settings and initialize structurs
		optimizingPlayer->UCT_param_IterNum = num_iterations_self;
		opponent->UCT_param_IterNum = num_iterations_opponent;
		opponent->UCT_param_C = fixed_CP_opponent;

		Player_Engine* players[2];
		players[eval_player_position] = optimizingPlayer;
		players[1 - eval_player_position] = opponent;

		Tom_Lrp* Lrp = new Tom_Lrp();
		Lrp->Initialize();
		Lrp->num_actions = funcApp1->num_actions;
		Lrp->Settings_Apply_New();
		Lrp->Evaluations_History_Allocate(num_LRP_iterations + 1, funcApp1->num_params);

		//results storage
		Tom_Sample_Storage<double>* score_history = new Tom_Sample_Storage<double>(num_LRP_iterations + 1);

		//srand((unsigned int)time(NULL));
		double cpu_time = getCPUTime();
		double cpu_time1;

		//calculate exponential time constant
		double dw_tau = (double)num_LRP_iterations / (log(dw_start / dw_limit));

		//output test settings
		if ((!silence_output) || (force_setting_output)){
			gmp->Print("LRP_improved_v1()\n");
			gmp->Print("%s %dx%d\n", game->game_name.c_str(), game->board_length, game->board_height);
			gmp->Print("Players: "); for (int i = 0; i < 2; i++) gmp->Print("%s \t", players[i]->player_name.c_str()); gmp->Print("\n");
			gmp->Print("Evaluated_player %d  sim %d  start_Cp %1.3f  par_w_init %1.3f\n", eval_player_position + 1, num_iterations_self, start_Cp_self, funcApp_init_weights);
			gmp->Print("Opponent_player  %d  sim %d  fixed_Cp %3.3f\n", 2 - eval_player_position, num_iterations_opponent, fixed_CP_opponent);
			gmp->Print("LRP_steps %5d  games %d %d\n", num_LRP_iterations, num_games_start, num_games_end);
			gmp->Print("LRP_restart %4d  restart_threshold %5.3f %5.3f\n", LRP_restart_condition_iterations, LRP_restart_condition_score_rel_diff, LRP_restart_condition_score_rel_diff_end);
			gmp->Print("kNN %2d  central_weight %3.1f  weight_repeats %d\n", LRP_best_neighbours_num, LRP_best_neighbour_weight_central, (int)KNN_weight_repeats);
			gmp->Print("enable_output_best_kNN %d  final_eval_games %d\n", (int)LRP_output_best_kNN, final_evaluation_num_games);
			gmp->Print("Score_conf_test %4.2f\n", lrp_score_diff_conf_factor);
			gmp->Print("Games_by_conf_fact %3.1f %3.1f\n", min_increase_num_games_fact, max_increase_num_games_fact);
			gmp->Print("LRP_ab_by_conf %d  LRP_dw_by_conf %d\n", (int)LRP_change_AB_by_confidence, (int)LRP_change_DW_by_confidence);
			gmp->Print("LRP_ab_min %1.3f  LRP_ab_max %1.3f  LRP_b_koef %1.3f\n", lrp_ab_min, lrp_ab_max, lrp_b_koef);
			gmp->Print("lrp_ab_dscore_min %1.3f lrp_ab_dscore_max %1.3f\n", lrp_ab_dscore_min, lrp_ab_dscore_max);
			gmp->Print("Use_linear_dw_decrease %d  dw %1.5f %1.5f  dw_tau %1.5f\n", (int)(!exp_dw_decrease), dw_start, dw_limit, dw_tau);
			gmp->Print("Evaluated_player:  RAVE_scheme %d  start_RAVE_V %4.1f\n", TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME, start_RAVE_V_self);
			gmp->Print("AMAF_EQUATION_TYPE %d  PARAM_FUNC_APP_TYPE %d  Nnet I %d O %d H %d F %d\n", TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE, TOMPLAYER_AI_UCT_TOMTEST_PARAM_FUNC_APP_TYPE, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_INPUTS, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_OUTPUTS, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_HIDD_LAYER, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NEURAL_FUNC);
			gmp->Print("QVALUE_EQUATION_TYPE %d  QVALUE_ALGORITHM %d  TRACE_GAMMA S%d T%d\n", TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_TRACE_GAMMA_SV, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_TRACE_GAMMA_TV);
			gmp->Print("\n");
		}

		//--- LRP algorithm ---//

		double score, previous_score, score_tmp, final_score;
		int num_LRP_batch_iterations, num_games_tmp;
		int selected_action;
		int index_best_setting;
		int count_restarts, count_restart_checks;
		double confidence_normal_dist1;
		double confidence_normal_dist2;
		double confidence_num_games1, confidence_num_games2;
		int total_games_count;

		double dw = dw_start;
		double lrp_ab = 0.0;
		int num_games = num_games_start;

		//evaluate starting setting
		score = game->Evaluate_Players(1, num_games, -1, players, false, eval_player_position) / num_games;
		score_history->Add_Sample(score);
		Lrp->Evaluations_History_Add_Sample(-1, num_games, score, 0.0, funcApp1->parameter_weights);
		total_games_count = num_games;

		//int best_iteration = -1;
		//double best_score = score;
		//double best_paramC = start_c1;

		if (!silence_output){
			if (show_full_output){
				gmp->Print("  \t     \t Parameters");
			}
			gmp->Print("\ni \t score");
			for (int p = 0; p < funcApp1->num_params; p++)
				gmp->Print("\t P%d", p);
			if (show_full_output){
				gmp->Print(" \t\t action");
				for (int a = 0; a < Lrp->num_actions; a++)
					gmp->Print("\t A%d[%%]", a);
				gmp->Print("\t dw \t\t lrp_ab");
				gmp->Print("\t Games");
				gmp->Print("\t NormDistConf");
			}
			gmp->Print("\n%d \t%5.1f%", 0, score * 100);
			for (int p = 0; p < funcApp1->num_params; p++)
				gmp->Print("\t%6.3f", funcApp1->parameter_weights[p]);
			if (show_full_output){
				gmp->Print(" \t\t ");
				gmp->Print("//");
				Lrp->Debug_Display_Probabilites(0);
				gmp->Print(" \t %1.5f \t %1.3f", dw, lrp_ab);
				gmp->Print(" \t %4d", num_games);
			}
			gmp->Print("\n");
		}

		//debug
		//double debug_scores[num_LRP_iterations] = {0.1 , 0.2 , 0.3 , 0.4 , 0.5};
		//srand(0);

		//Lrp iterations
		num_LRP_batch_iterations = 0;
		count_restarts = 0;
		count_restart_checks = 0;
		for (int i = 0; i < num_LRP_iterations; i++){

			if (exp_dw_decrease){
				//exponential iterative decrease of weight change step
				dw = dw_start * exp(-i / dw_tau);
			}
			else{
				//linear iterative decrease of weight change step
				dw = dw_limit + (dw_start - dw_limit) * (1.0 - (double)i / (num_LRP_iterations - 1));
			}

			//increase number of games per evalation

			num_games = (int)((num_games_start + (num_games_end - num_games_start) * ((double)(i + 1) / num_LRP_iterations)) + 0.5);

			//Lrp select action
			selected_action = Lrp->Select_Action(TOM_LRP_SELECT_ACTION_CHECK_SUM);

			//apply action
			//if(selected_action == Lrp->num_actions-1){
			//	funcApp1->parameter_weights[0] = 0.2;
			//}else{
			//	funcApp1->parameter_weights[0] = -0.5;
			//}
			funcApp1->Action_Update_Weights(selected_action, dw);
			//optimizingPlayer->UCT_param_C = funcApp1->parameter_weights[0];

			//evaluate new setting and save scores
			previous_score = score;						//save previous score
			score = game->Evaluate_Players(1, num_games, -1, players, false, eval_player_position) / num_games;

			//-- statistical difference test --//
			int stat_run_once = 0;
			while (stat_run_once < 2){

				//calculate the confidence, that the last two scores belong to different normal distributions
				confidence_normal_dist1 =
					sqrt(
					(Lrp->evaluations_history[Lrp->evaluations_history_num_samples - 1].num_repeats)
					* (score - previous_score) * (score - previous_score)
					/ (previous_score * (1 - previous_score) + 0.00001)	// + 0.00001 only to avoid division by 0
					/ 2.0 //optional
					);
				//calculate the confidence, that the last two scores belong to different normal distributions
				confidence_normal_dist2 =
					sqrt(
					(num_games)
					* (score - previous_score) * (score - previous_score)
					/ (score * (1 - score) + 0.00001)	// + 0.00001 only to avoid division by 0
					/ 2.0 //optional
					);
				confidence_num_games1 = lrp_score_diff_conf_factor*lrp_score_diff_conf_factor*(previous_score * (1 - previous_score)) * 2.0 / ((score - previous_score) * (score - previous_score) + 0.00001);
				confidence_num_games2 = lrp_score_diff_conf_factor*lrp_score_diff_conf_factor*(score * (1 - score)) * 2.0 / ((score - previous_score) * (score - previous_score) + 0.00001);

				//re-evaluate with higher number of games if confidence below threshold (lrp_score_diff_conf_factor)
				num_games_tmp = (int)(((max_increase_num_games_fact - 1.0)*num_games) * (1.0 - min(1.0, max(confidence_normal_dist1, confidence_normal_dist2) / lrp_score_diff_conf_factor)));
				if ((num_games_tmp > 0) && (stat_run_once == 0)){										//if confidence below threshold, then repeat player evaluation with higher number of games
					num_games_tmp = (int)max(num_games_tmp, num_games*(min_increase_num_games_fact - 1.0));		//set number of additional games minimum to increase_num_games_thr
					score_tmp = game->Evaluate_Players(1, num_games_tmp, -1, players, false, eval_player_position) / num_games_tmp;	//perform additional evaluations
					score = (score_tmp*num_games_tmp + score*num_games) / (num_games_tmp + num_games);	//update score
					num_games += num_games_tmp;		//update number of evaluation games
					stat_run_once = 1;	//repeat statistical test
				}
				else{
					stat_run_once = 2;	//do not repeat statistical test
				}

			}
			//-- END - statistical difference test --//

			score_history->Add_Sample(score);
			Lrp->Evaluations_History_Add_Sample(selected_action, num_games, score, 0.0, funcApp1->parameter_weights);
			total_games_count += num_games;

			//score = debug_scores[i]; //DEBUG

			if (!silence_output){
				gmp->Print("%d \t%5.1f%", i + 1, score * 100);
				for (int p = 0; p < funcApp1->num_params; p++)
					gmp->Print("\t%6.3f", funcApp1->parameter_weights[p]);
				gmp->Print(" \t\t ");
			}

			//-- adaptively change LRP learning parameter alpha (and beta) --//
			//proportional to confidence between scores
			if (LRP_change_AB_by_confidence){

				lrp_ab = max(lrp_ab_min, lrp_ab_max * min(1.0, max(confidence_normal_dist1, confidence_normal_dist2) / lrp_score_diff_conf_factor));
				//inverse, for testing only - should perform worse //lrp_ab = lrp_ab_min + lrp_ab_max - max(lrp_ab_min, lrp_ab_max * min(1.0 , max(confidence_normal_dist1, confidence_normal_dist2) / 1.96));

				//linearly proportional lrp learning parameter
			}
			else{
				lrp_ab =
					lrp_ab_min + (lrp_ab_max - lrp_ab_min) * max(0.0, min(1.0,
					(abs(score - previous_score) - lrp_ab_dscore_min) / (lrp_ab_dscore_max - lrp_ab_dscore_min)
					));
			}
			Lrp->learn_param_a = lrp_ab;
			Lrp->learn_param_b = lrp_ab * lrp_b_koef;
			//-- END - adaptively change LRP learning parameter alpha (and beta) --//

			//Lrp correct probabilites based on previous and current score
			if (score > previous_score){
				Lrp->Update_Probabilites_Reward(selected_action, Lrp->learn_param_a);
				if (!silence_output) if (show_full_output) gmp->Print("+");
			}
			else{
				Lrp->Update_Probabilites_Penalty(selected_action, Lrp->learn_param_b);
				if (!silence_output) if (show_full_output) gmp->Print("-");
			}

			//output
			if (!silence_output){
				if (show_full_output){
					gmp->Print("%d", selected_action);
					Lrp->Debug_Display_Probabilites(0);
					gmp->Print(" \t %1.5f \t %1.3f", dw, lrp_ab);
					gmp->Print(" \t %4d", num_games);
					gmp->Print(" \t %5.2f %5.2f %4.0f %4.0f", confidence_normal_dist1, confidence_normal_dist2, confidence_num_games1, confidence_num_games2);
				}
			}
			//remember best
			//if(score >= best_score){
			//	best_iteration = i;
			//	best_score = score;
			//	best_paramC = playerUCT1->UCT_param_C;
			//}

			//Restart from best position periodically after LRP_restart_condition_iterations iterations
			num_LRP_batch_iterations++;
			if ((LRP_restart_condition_iterations > 0) && (num_LRP_batch_iterations >= LRP_restart_condition_iterations)){

				//calculate kNN scores and find best parameter setting
				index_best_setting = Lrp->Evaluations_History_Best_KNN_Average_Slow(LRP_best_neighbours_num, KNN_weight_repeats, LRP_best_neighbour_weight_central, false, 0);

				//check difference between scores of last sample (current state) and best score, threshold decreases linearly (by increasing number of iterations, it is more likely to restart from best position)
				double last_sample_score = Lrp->evaluations_history[(Lrp->evaluations_history_num_samples) - 1].kNN_score;
				double best_sample_score = Lrp->evaluations_history[index_best_setting].kNN_score;
				double current_score_ratio;
				if (best_sample_score != 0.0)	//division safety
					current_score_ratio = 1.0 - last_sample_score / best_sample_score;
				else
					current_score_ratio = 0.0;
				double restart_threshold = LRP_restart_condition_score_rel_diff_end + (LRP_restart_condition_score_rel_diff - LRP_restart_condition_score_rel_diff_end) * (1.0 - (double)i / (num_LRP_iterations - 1));

				//DEBUG
				//gmp->Print("\n"); Lrp->Debug_Display_Evaluations_History();
				//gmp->Print("\t %d %f %d %f\t",index_best_setting, best_sample_score, (Lrp->evaluations_history_num_samples)-1, last_sample_score);

				if (current_score_ratio >= restart_threshold){

					if (!silence_output){
						gmp->Print(" \t RESTART %3.3f", current_score_ratio);
					}

					//--do execute restart from best position--

					//reset LRP probabilites
					Lrp->State_Reset();

					//set parameter values to best combination found so far
					for (int p = 0; p < funcApp1->num_params; p++)
						funcApp1->parameter_weights[p] = Lrp->evaluations_history[index_best_setting].param_values[p];

					//set appropriate score to compare in next iteration
					score = Lrp->evaluations_history[index_best_setting].score;

					//increase counter of restarts
					count_restarts++;
				}
				else{
					if (!silence_output){
						gmp->Print(" \t CHECK %3.3f", current_score_ratio);
					}
				}

				num_LRP_batch_iterations = 0;
				count_restart_checks++;
			}

			if (!silence_output){
				gmp->Print("\n");
			}
		}


		//output best parameter combination from history instead of values in last iteration
		if (LRP_output_best_kNN){
			index_best_setting = Lrp->Evaluations_History_Best_KNN_Average_Slow(LRP_best_neighbours_num, KNN_weight_repeats, LRP_best_neighbour_weight_central, false, 0);
			for (int p = 0; p < funcApp1->num_params; p++)
				funcApp1->parameter_weights[p] = Lrp->evaluations_history[index_best_setting].param_values[p];
		}

		//--- LRP algorithm END---//

		cpu_time = getCPUTime() - cpu_time;

		//evaluation of final parameters
		optimizingPlayer->UCT_param_IterNum = final_evaluation_num_iterations_self;
		opponent->UCT_param_IterNum = final_evaluation_num_iterations_opponent;
		players[final_eval_player_position] = optimizingPlayer;
		players[1 - final_eval_player_position] = opponent;
		cpu_time1 = getCPUTime();
		final_score = game->Evaluate_Players(1, final_evaluation_num_games, -1, players, false, eval_player_position) / final_evaluation_num_games;
		cpu_time1 = getCPUTime() - cpu_time;

		//output best
		//gmp->Print("\nBest score: %5.1f% \t P1->Cp  %5.3f \t i  %d\n", best_score*100, best_paramC, best_iteration+1);

		//output values
		score_history->Calc_AvgDev();
		double score10 = score_history->Calc_Avg_On_Interval((int)(num_LRP_iterations*0.9), num_LRP_iterations);
		if (!silence_output){

			double final_eval_dev = sqrt(1.96*1.96*final_score*(1 - final_score)*2.0 / final_evaluation_num_games);

			//Lrp output (best parameters value)
			gmp->Print("\n");
			//Lrp->Debug_Display_Evaluations_History();	//output LRP evaluations history
			Lrp->Evaluations_History_Best_Single();
			Lrp->Evaluations_History_Best_KNN_Average_Slow(LRP_best_neighbours_num, true, LRP_best_neighbour_weight_central, false);

			//Statistics and final evaluation
			gmp->Print("\navgGam \t avg100\t avg10 \t fEval", final_evaluation_num_games);
			for (int p = 0; p < funcApp1->num_params; p++)
				gmp->Print("\t FinP%d", p);
			gmp->Print("\t fEvaConf95");
			gmp->Print("\n%6.1f \t %5.2f \t %5.2f \t %5.2f", (double)total_games_count / (num_LRP_iterations + 1), score_history->avg * 100, score10 * 100, final_score * 100);
			for (int p = 0; p < funcApp1->num_params; p++)
				gmp->Print("\t%7.4f", funcApp1->parameter_weights[p]);
			gmp->Print("\t %5.2f", final_eval_dev * 100);

			//runtime
			gmp->Print("\n\nRuntime [s]:\n  Lrp\t%9.3f\n  Eval\t%9.3f (%d games)\n  Total\t%9.3f\n", cpu_time, cpu_time1, final_evaluation_num_games, cpu_time + cpu_time1);
		}


		//DEBUG
		//gmp->Print("RR: %5.2f%% \t ",(double)count_restarts/count_restart_checks*100.0);	//output restart rate
		//gmp->Print("ratio p0: %3.3f\n",optimizingPlayer->debug_dbl_cnt1 / optimizingPlayer->debug_dbl_cnt2 *100.0);	//2-interval C search ratio of use p0 to (p0+p1)

		//return values
		if (score_avg != NULL)
			(*score_avg) = score_history->Calc_Avg();
		if (last_param_val != NULL)
		for (int i = 0; i < funcApp1->num_params; i++)
			(*last_param_val)[i] = funcApp1->parameter_weights[i];
		if (score_avg_last10 != NULL)
			(*score_avg_last10) = score_history->Calc_Avg_On_Interval((int)(num_LRP_iterations*0.9), num_LRP_iterations);
		if (avg_num_games != NULL)
			(*avg_num_games) = (double)total_games_count / (num_LRP_iterations + 1);
		if (final_eval_score != NULL)
			(*final_eval_score) = final_score;

		gmp->Flush();

		//free memory
		delete(score_history);
		delete(Lrp);
		delete(optimizingPlayer);
		delete(opponent);
		delete(game);
	}

	void LRP_test_wrapperMultiPar()
	{

		const int num_repeats = 100;				//number of LRP repeats (not STEPS!)
		const int num_LRP_params = TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_PARAMS;			//the number of weights set by LRP
		const int num_final_evaluations = 100000;	//number of games to evaluate final combination of parameters (default 750)
		//20000, 95% confidence that true value deviates less by 1%
		//5000,  95% confidence that true value deviates less by 2%
		//1200,  95% confidence that true value deviates less by 4%
		//750,   95% confidence that true value deviates less by 5%
		//200,	 95% confidence that true value deviates less by 10%
		// --- settings end --- //

		double score_avg;
		double score_avg_last10;
		double* last_param_val = new double[num_LRP_params];
		double avg_num_games;
		double final_eval_score;

		bool output_settings_once = true;

		double cpu_time = getCPUTime();

		Tom_Sample_Storage<double>* avg_score = new Tom_Sample_Storage<double>(num_repeats);
		Tom_Sample_Storage<double>* avg_score10 = new Tom_Sample_Storage<double>(num_repeats);
		Tom_Sample_Storage<double>* last_param[num_LRP_params];
		for (int p = 0; p < num_LRP_params; p++)
			last_param[p] = new Tom_Sample_Storage<double>(num_repeats);
		Tom_Sample_Storage<double>* final_score = new Tom_Sample_Storage<double>(num_repeats);
		Tom_Sample_Storage<double>* num_games_per_step = new Tom_Sample_Storage<double>(num_repeats);

		gmp->Print("LRP_test_wrapperMultiPar(), %d repeats %d params\n\n", num_repeats, num_LRP_params);

		for (int r = 0; r < num_repeats; r++){
			//LRP_test_linAB_exponentDW_FunApp_MulParams(&score_avg,&score_avg_last10,&last_param_val,output_settings_once,num_LRP_params);
			LRP_improved_v1(&score_avg, &score_avg_last10, &last_param_val, output_settings_once, num_final_evaluations, &avg_num_games, &final_eval_score);

			if (output_settings_once){
				gmp->Print("\nRun       avg    [%%]    [%%]    [%%]   Final_params\n");
				gmp->Print("        games scr100  scr10  final   "); for (int p = 0; p < num_LRP_params; p++) gmp->Print("P%d       ", p); gmp->Print("\n");
			}

			gmp->Print("%3d    %6.1f %6.2f %6.2f %6.2f  ", r, avg_num_games, score_avg * 100, score_avg_last10 * 100, final_eval_score * 100); for (int p = 0; p < num_LRP_params; p++) gmp->Print("%7.4f  ", last_param_val[p]); gmp->Print("\n");
			avg_score->Add_Sample(score_avg);
			avg_score10->Add_Sample(score_avg_last10);
			for (int p = 0; p < num_LRP_params; p++)
				last_param[p]->Add_Sample(last_param_val[p]);
			final_score->Add_Sample(final_eval_score);
			num_games_per_step->Add_Sample(avg_num_games);

			output_settings_once = false;

			gmp->Flush();
		}

		avg_score->Calc_AllExceptSum();
		avg_score10->Calc_AllExceptSum();
		for (int p = 0; p < num_LRP_params; p++){
			last_param[p]->Calc_AllExceptSum();
		}
		final_score->Calc_AllExceptSum();
		num_games_per_step->Calc_AllExceptSum();

		gmp->Print("\n\n");

		gmp->Print("Summary   avg    [%%]    [%%]    [%%]   Final_params\n");
		gmp->Print("        games scr100  scr10  final   "); for (int p = 0; p < num_LRP_params; p++) gmp->Print("P%d       ", p); gmp->Print("\n");
		gmp->Print("avg    %6.1f %6.2f %6.2f %6.2f  ", num_games_per_step->avg, avg_score->avg * 100, avg_score10->avg * 100, final_score->avg * 100); for (int p = 0; p < num_LRP_params; p++) gmp->Print("%7.4f  ", last_param[p]->avg); gmp->Print("\n");
		gmp->Print("dev    %6.1f %6.2f %6.2f %6.2f  ", num_games_per_step->dev, avg_score->dev * 100, avg_score10->dev * 100, final_score->dev * 100); for (int p = 0; p < num_LRP_params; p++) gmp->Print("%7.4f  ", last_param[p]->dev); gmp->Print("\n");
		gmp->Print("conf   %6.1f %6.2f %6.2f %6.2f  ", num_games_per_step->conf, avg_score->conf * 100, avg_score10->conf * 100, final_score->conf * 100); for (int p = 0; p < num_LRP_params; p++) gmp->Print("%7.4f  ", last_param[p]->conf); gmp->Print("\n");
		gmp->Print("median %6.1f %6.2f %6.2f %6.2f  ", num_games_per_step->median, avg_score->median * 100, avg_score10->median * 100, final_score->median * 100); for (int p = 0; p < num_LRP_params; p++) gmp->Print("%7.4f  ", last_param[p]->median); gmp->Print("\n");

		cpu_time = getCPUTime() - cpu_time;
		gmp->Print("\n\nTotalRuntime: %9.3f s\n", cpu_time);
		gmp->Flush();

		delete[] last_param_val;
		delete(avg_score);
		delete(avg_score10);
		for (int p = 0; p < num_LRP_params; p++)
			delete(last_param[p]);
	}

	void Fixed_Play_Testing(double input_param1, double input_param2)
	{

		//--- SET GAME AND PLAYERS HERE ---//

		//Game_Engine* game = new Game_ConnectFour();
		//Game_Engine* game = new Game_Gomoku();
		Game_Engine* game = new Game_TicTacToe();
		//Game_Engine* game = new Game_Hex();

		////variable board size
		//if(input_param1 > 0){
		//	game->board_length = (int)input_param1;
		//	game->board_height = (int)input_param1;
		//	game->board_size = game->board_length * game->board_height;
		//	game->maximum_allowed_moves = game->board_size;
		//	game->maximum_plys = game->board_size;
		//	game->Settings_Apply_Changes();
		//}

		Player_Engine* players[2];

		Player_AI_UCT* opponent = new Player_AI_UCT(game);
		//Player_AI_UCT_RAVE* opponent = new Player_AI_UCT_RAVE(game);
		Player_AI_UCT* benchmarkOpponent = new Player_AI_UCT(game);
		Player_AI_UCT_TomTest* evaluated = new Player_AI_UCT_TomTest();
		Player_AI_UCT_TomTest* benchmarkEvaluated = new Player_AI_UCT_TomTest();

		//--- pre-init (no need to change this) ---//

		Tom_Function_Approximator* funcApp1 = new Tom_Function_Approximator();
		funcApp1->Initialize();
		funcApp1->num_results = TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_PARAMS;
		funcApp1->Settings_Apply_New();

		evaluated->Func_App_UCT_Params = funcApp1;
		evaluated->game = game;
		benchmarkEvaluated->Func_App_UCT_Params = funcApp1;
		benchmarkEvaluated->game = game;

		evaluated->Initialize();
		benchmarkEvaluated->Initialize();
		evaluated->Output_Log_Level_Create_Headers();

		//--- SET BENCHMARK PARAMETERS HERE ---//

		int repeats = 100000;
		//1000000, 95% confidence that true value deviates less by 0.1%
		//200000,  95% confidence that true value deviates less by 0.3%
		//20000,   95% confidence that true value deviates less by 1%
		//5000,    95% confidence that true value deviates less by 2%
		//1200,    95% confidence that true value deviates less by 4%

		int eval_player_position = 1;			//which player to test: 0 or 1; works only for two-player games
		int benchmark_same_player = 0;
		//0 - disabled (default)
		//1 - benchmark "opponent" vs "opponent"
		//2 - benchmark "evaluated" vs "evaluated"
		int human_opponent_override = 0;
		//0 - disabled (default)
		//1 - replace opponent AI player with human input and output game state after every move
		int display_output_game = 0;	//display single output game regardless of human or AI opponent
		int measure_time_per_move = 0;	//output time per move for each player at end of evaluation procedure
		//int output_interval_repeats = (int)(sqrt(repeats));	//output interval of average values from evaluation function (0 = disabled)
		int output_interval_repeats = 1;
		//int output_interval_repeats = 0;

		//--- SET PLAYERS PARAMETERS HERE ---//

		//opponent->UCT_param_IterNum = (int)input_param1;
		opponent->UCT_param_IterNum = 10;
		opponent->UCT_param_C = 0.2;//0.5/sqrt(2);
		//opponent->UCT_param_C = -0.08;
		//opponent->RAVE_param_V = 37;

		//evaluated->UCT_param_IterNum = (int)input_param1;
		//evaluated->UCT_param_IterNum = (int)input_param2;
		evaluated->UCT_param_IterNum = 10;
		//evaluated->UCT_param_C = 0.15;//0.5/sqrt(2);

		////2014_12_18 TTT TDfourth 10 iter as player2 win ~35-36% (exactly the same as the LAST line in "_2014_03_11 paper - config.txt")
		//evaluated->UCT_param_C = 0.0035;
		//evaluated->QVALUE_param_scoreWeight = 0.0;
		//evaluated->QVALUE_param_alpha = 1.0;
		//evaluated->QVALUE_param_gamma = 0.9;
		//evaluated->QVALUE_param_lambda = 0.9;
		//evaluated->QVALUE_param_initVal = 0.0;

		////2014_12_19 correctedQtrace TTT 10iter TDfourth ~ 41% win rate P2
		evaluated->UCT_param_C = 0.0119;
		evaluated->QVALUE_param_scoreWeight = 0.7900;
		evaluated->QVALUE_param_alpha = 1.9152;
		evaluated->QVALUE_param_gamma = 0.2351;
		evaluated->QVALUE_param_lambda = 1.4491;
		evaluated->QVALUE_param_initVal = 0.0;

		////2014_12_19 correctedQtrace TTT 10iter TDfourth leafVal ~ 39% win rate P2
		//evaluated->UCT_param_C = 0.4273;
		//evaluated->QVALUE_param_scoreWeight = -3.8674;
		//evaluated->QVALUE_param_alpha = -1.6693;
		//evaluated->QVALUE_param_gamma = 0.4520;
		//evaluated->QVALUE_param_lambda = 1.3549;
		//evaluated->QVALUE_param_initVal = 0.0;

		//evaluated->RAVE_param_V = 35;
		//evaluated->UCT_param_C = -5.7522;		

		//2014_01 Q-Values test: best 11 param LRP search settings (TOMPLAYER_AI_UCT_TOMTEST_QVALUE_TRACE_GAMMA_TV = 1) 
		//GOMOKU
		//evaluated->UCT_param_C = 0.079;
		//evaluated->QVALUE_param_scoreWeight = 0.395;
		//evaluated->QVALUE_param_alpha = 1.229;
		//evaluated->QVALUE_param_gamma = 1.739;
		//evaluated->QVALUE_param_lambda = 0.125;
		//evaluated->QVALUE_param_lambda_playOut = 0.843;
		//evaluated->QVALUE_param_leaf_node_val0 = 0.132;
		//evaluated->QVALUE_param_leaf_node_val1 = 2.023;
		//evaluated->QVALUE_param_leaf_node_val2 = -1.266;
		//evaluated->QVALUE_param_leaf_node_val3 = 1.540;
		//evaluated->QVALUE_param_initVal = 0.407;

		//-- 2014_02_19 eval 1M Gomoku comparison best --//
		//-- 2014_03_11 paper -//

		//evaluated->UCT_param_C = 0.04;	//TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 0, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0
		////
		//evaluated->UCT_param_C = -0.08;
		//evaluated->RAVE_param_V = 37.0;	//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 1, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 1, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0

		//evaluated->UCT_param_C = 0.07;
		//evaluated->AMAF_param_alpha = 0.43;	//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 3, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0

		//evaluated->AMAF_param_alpha = -1.5;	//set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 2, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM = 3

		//evaluated->QVALUE_param_scoreWeight = 0.6;
		//evaluated->AMAF_param_alpha = -1.75;	//set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 2, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM = 3

		//evaluated->UCT_param_C = -5.3042;		//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 3, set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 2, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM = 3
		//evaluated->QVALUE_param_scoreWeight = 2.9037;	
		//evaluated->AMAF_param_alpha = 0.9169;	

		//CONNECT4
		//evaluated->UCT_param_C = 0.122;
		//evaluated->QVALUE_param_scoreWeight = 0.141;
		//evaluated->QVALUE_param_alpha = 0.744;
		//evaluated->QVALUE_param_gamma = 1.229;
		//evaluated->QVALUE_param_lambda = 0.466;
		//evaluated->QVALUE_param_lambda_playOut = 0.815;
		//evaluated->QVALUE_param_leaf_node_val0 = 2.136;
		//evaluated->QVALUE_param_leaf_node_val1 = 1.099;
		//evaluated->QVALUE_param_leaf_node_val2 = -0.561;
		//evaluated->QVALUE_param_leaf_node_val3 = 3.875;
		//evaluated->QVALUE_param_initVal = 1.366;

		//-- 2014_02_19 end --//

		//-- 2014_02_24 eval 1M Conn4 comparison best --//
		//evaluated->UCT_param_C = 0.177;

		//evaluated->UCT_param_C = 0.170;
		//evaluated->AMAF_param_alpha =-0.986;	//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 1, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 0, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0

		//evaluated->UCT_param_C = 0.190;
		//evaluated->RAVE_param_V = -85.550;	//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 1, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 1, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0

		//evaluated->UCT_param_C = 0.150;
		//evaluated->AMAF_param_alpha =-0.930;	//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 3, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 0, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0

		//evaluated->UCT_param_C = 0.150;		//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 1, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 0, set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 2, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM = 3
		//evaluated->QVALUE_param_scoreWeight = 0.120;	
		//evaluated->AMAF_param_alpha = -0.792;	

		//evaluated->UCT_param_C = 0.147;		//set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 3, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 0, set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 2, TOMPLAYER_AI_UCT_TOMTEST_QVALUE_ALGORITHM = 3
		//evaluated->QVALUE_param_scoreWeight = 0.133;	
		//evaluated->AMAF_param_alpha = -0.699;	

		////benchmark 2014_02_24 Conn4 comparison best - 5
		////set TOMPLAYER_AI_UCT_TOMTEST_AMAF_EQUATION_TYPE = 1, TOMPLAYER_AI_UCT_TOMTEST_AMAF_ENABLE_RAVE_SCHEME = 1, set TOMPLAYER_AI_UCT_TOMTEST_QVALUE_EQUATION_TYPE = 0
		////set TOMPLAYER_AI_UCT_TOMTEST_PARAM_FUNC_APP_TYPE = 3, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_INPUTS 3, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_NUM_OUTPUTS 2, TOMPLAYER_AI_UCT_TOMTEST_FUNC_APPROX_HIDD_LAYER 3
		//evaluated->UCT_param_C = 0.2;
		//evaluated->RAVE_param_V = 0.0;	
		//funcApp1->parameter_weights[0] = 0.0050;
		//funcApp1->parameter_weights[1] = -2.2469;
		//funcApp1->parameter_weights[2] = -0.9316;
		//funcApp1->parameter_weights[3] = 1.2891;
		//funcApp1->parameter_weights[4] = -0.2774;
		//funcApp1->parameter_weights[5] = -1.5939;
		//funcApp1->parameter_weights[6] = 0.3289;
		//funcApp1->parameter_weights[7] = 1.3833;
		//funcApp1->parameter_weights[8] = -1.8694;
		//funcApp1->parameter_weights[9] = 2.4987;
		//funcApp1->parameter_weights[10] = 0.5779;
		//funcApp1->parameter_weights[11] = -0.3715;
		//funcApp1->parameter_weights[12] = 0.5555;
		//funcApp1->parameter_weights[13] = 0.9744;
		//funcApp1->parameter_weights[14] = 1.0207;
		//funcApp1->parameter_weights[15] = 0.3399;
		//funcApp1->parameter_weights[16] = -0.5799;
		//funcApp1->parameter_weights[17] = -0.0008;
		//funcApp1->parameter_weights[18] = 1.4078;
		//funcApp1->parameter_weights[19] = 0.3903;

		//-- 2014_03_03 SARSA+AMAF_EQ3 direct search 12 par - configuration
		//	//gomoku
		//evaluated->UCT_param_C = -11.7952;		
		//evaluated->AMAF_param_alpha = 0.82822;	
		//evaluated->QVALUE_param_scoreWeight = 2.2312;	
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 1.0004;
		//evaluated->QVALUE_param_lambda = 0.6664;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;
		////	//connect4
		//evaluated->UCT_param_C = 0.1747;		
		//evaluated->AMAF_param_alpha = -0.7338;	
		//evaluated->QVALUE_param_scoreWeight = 0.3131;	
		//evaluated->QVALUE_param_alpha = 0.7833;
		//evaluated->QVALUE_param_gamma = 1.0839;
		//evaluated->QVALUE_param_lambda = 0.397;
		//evaluated->QVALUE_param_lambda_playOut = 0.8754;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		////-- 2014_03_11 paper T1 SARSA (no AMAF)
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;
		//evaluated->UCT_param_C = 0.0544;		
		//evaluated->QVALUE_param_scoreWeight = 0.7036;	
		//evaluated->QVALUE_param_alpha = 0.7209;
		//evaluated->QVALUE_param_gamma = 0.9072;
		//evaluated->QVALUE_param_lambda = 0.6071;
		//evaluated->QVALUE_param_lambda_playOut = 0.85812;

		//-- 2014_03_11 paper T1 SARSA no MC (and no AMAF)
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;
		//evaluated->UCT_param_C = 0.1703;		
		//evaluated->QVALUE_param_scoreWeight = 1.0;	
		//evaluated->QVALUE_param_alpha = 0.2218;
		//evaluated->QVALUE_param_gamma = 0.708;
		//evaluated->QVALUE_param_lambda = 1.6706;
		//evaluated->QVALUE_param_lambda_playOut = 0.9141;

		//// SARSA+AMAF 
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;
		//evaluated->UCT_param_C = -5.75;
		//evaluated->AMAF_param_alpha = 0.90;
		//evaluated->QVALUE_param_scoreWeight = 3.51;
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 1.0004;
		//evaluated->QVALUE_param_lambda = 0.6664;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;

		////SARSA-CORRECT gamma 1 run2 alpha 1 Wq 1
		//evaluated->UCT_param_C = 0.045;		
		//evaluated->QVALUE_param_scoreWeight = 1.0;	
		//evaluated->QVALUE_param_alpha = 1.0;
		//evaluated->QVALUE_param_gamma = 1.0;
		//evaluated->QVALUE_param_lambda = 0.5276;
		//evaluated->QVALUE_param_lambda_playOut = 0.92086;
		//evaluated->QVALUE_param_initVal = 0.5;

		//SARSA-CORRECT gamma 1 run2 (and variants)
		//optimizingPlayer->UCT_param_C = 0.0227;		
		//optimizingPlayer->QVALUE_param_scoreWeight = 1.0;	
		//optimizingPlayer->QVALUE_param_alpha = 0.9912;
		//optimizingPlayer->QVALUE_param_gamma = 1.0;
		//optimizingPlayer->QVALUE_param_lambda = 0.6206;
		//optimizingPlayer->QVALUE_param_lambda_playOut = 0.85856;
		//optimizingPlayer->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run 2 + AMAF_EQ3
		//evaluated->UCT_param_C = -2.6226;
		//evaluated->AMAF_param_alpha = 0.88664;
		//evaluated->QVALUE_param_scoreWeight = 1.3173;	
		//evaluated->QVALUE_param_alpha = 1.0;
		//evaluated->QVALUE_param_gamma = 1.0;
		//evaluated->QVALUE_param_lambda = 0.8118;
		//evaluated->QVALUE_param_lambda_playOut = 0.8662;
		//evaluated->QVALUE_param_initVal = 0.5;

		////SARSA-CORRECT run5 - individual parameters
		//evaluated->UCT_param_C = 0.04944;
		//evaluated->QVALUE_param_scoreWeight = 0.7943;
		//evaluated->QVALUE_param_alpha = 1.111;
		//evaluated->QVALUE_param_gamma = 1.0;
		//evaluated->QVALUE_param_lambda = 0.69502;
		//evaluated->QVALUE_param_lambda_playOut = 0.8853;
		//evaluated->QVALUE_param_initVal = 0.5;

		////SARSA-third test1
		//evaluated->UCT_param_C = 0.05;
		//evaluated->QVALUE_param_scoreWeight = 1.0;
		//evaluated->QVALUE_param_alpha = 1.0;
		//evaluated->QVALUE_param_gamma = 1.0;
		//evaluated->QVALUE_param_lambda = 1.0;
		//evaluated->QVALUE_param_initVal = 0.0;

		////SARSAcorrect variant0
		//evaluated->UCT_param_C = 0.049;
		//evaluated->QVALUE_param_scoreWeight = 0.794;
		//evaluated->QVALUE_param_alpha = 1.0;
		//evaluated->QVALUE_param_gamma = 1.0;
		//evaluated->QVALUE_param_lambda = 0.885;
		//evaluated->QVALUE_param_lambda_playOut = 0.885;
		//evaluated->QVALUE_param_initVal = 0.5;

		////TDfourth - TDleaf
		////double TD_leaf_state_value = currentNode->Q_value;;
		////MC_value = set_QVALUE_param_scoreWeight*(child->Q_value+0.5) + set_MC_weight*(MC_value);	//TDfourth
		//evaluated->UCT_param_C = 0.022;
		//evaluated->QVALUE_param_scoreWeight = 0.863;
		//evaluated->QVALUE_param_alpha = 0.670;
		//evaluated->QVALUE_param_gamma = 0.845;
		//evaluated->QVALUE_param_lambda = 0.663;
		//evaluated->QVALUE_param_initVal = 0.0;

		////TDfourth - TD0
		////double TD_leaf_state_value = QVALUE_param_initVal;
		////MC_value = set_QVALUE_param_scoreWeight*(child->Q_value+0.5) + set_MC_weight*(MC_value);	//TDfourth
		//evaluated->UCT_param_C = 0.049;
		//evaluated->QVALUE_param_scoreWeight = 0.794;
		//evaluated->QVALUE_param_alpha = 1.111;
		//evaluated->QVALUE_param_gamma = 0.885;
		//evaluated->QVALUE_param_lambda = 0.785;
		//evaluated->QVALUE_param_initVal = 0.0;

		////TDfourth - TDmin
		////double TD_leaf_state_value = currentNode->Q_value;;
		////MC_value = set_QVALUE_param_scoreWeight*(child->Q_value+0.5) + set_MC_weight*(MC_value);	//TDfourth
		//evaluated->UCT_param_C = 0.052;
		//evaluated->QVALUE_param_scoreWeight = 1.0;
		//evaluated->QVALUE_param_alpha = 1.0;
		//evaluated->QVALUE_param_gamma = 0.877;
		//evaluated->QVALUE_param_lambda = 1.0;
		//evaluated->QVALUE_param_initVal = 0.0;

		////TDfourth - remake SARSA first version
		//evaluated->UCT_param_C = -5.75;
		//evaluated->AMAF_param_alpha = 0.90;
		//evaluated->QVALUE_param_scoreWeight = 3.51;
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 0.9129;
		//evaluated->QVALUE_param_lambda = 0.6664;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;
		//evaluated->QVALUE_param_initVal = 0.0;

		////  SARSA(lambda) two + AMAF
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;
		//evaluated->UCT_param_C = -5.75;
		//evaluated->AMAF_param_alpha = 0.90;
		//evaluated->QVALUE_param_scoreWeight = 3.51;
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 0.9129;
		//evaluated->QVALUE_param_lambda = 0.730;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;

		//-- 2014_03_11-17 paper F4o2 1000 iterations Gomo7x7 best params at 1-3 param search
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		//// UCB
		//evaluated->UCT_param_C = 0.1022;

		//// RAVE
		//evaluated->UCT_param_C = 0.1382;
		//evaluated->RAVE_param_V = -149.77;

		// SARSA
		//evaluated->UCT_param_C = 0.1024;		
		//evaluated->QVALUE_param_scoreWeight = 0.2152;	
		//evaluated->QVALUE_param_alpha = 0.7209;
		//evaluated->QVALUE_param_gamma = 0.9072;
		//evaluated->QVALUE_param_lambda = 0.6071;
		//evaluated->QVALUE_param_lambda_playOut = 0.85812;

		//// SARSA+AMAF 
		//evaluated->UCT_param_C = -13.931;
		//evaluated->AMAF_param_alpha = 0.97942;
		//evaluated->QVALUE_param_scoreWeight = 0.9208;
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 1.0004;
		//evaluated->QVALUE_param_lambda = 0.6664;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;

		//-- end - 2014_03_11 paper T11000 iterations Gomo7x7 best params


		//-- 2014_04_01 paper F4 optimized values at 1000 iterations Gomo 7x7 to 19x19

		////-- Gomoku 9x9 iterations 1000-1000 --//
		////Plain UCT
		//evaluated->UCT_param_C = 0.052;

		////RAVE
		//evaluated->UCT_param_C = -0.149;
		//evaluated->RAVE_param_V = 332;

		////SARSA
		//evaluated->UCT_param_C = 0.086;		
		//evaluated->QVALUE_param_scoreWeight = 0.520;	
		//evaluated->QVALUE_param_alpha = 0.7209;
		//evaluated->QVALUE_param_gamma = 0.9072;
		//evaluated->QVALUE_param_lambda = 0.6071;
		//evaluated->QVALUE_param_lambda_playOut = 0.85812;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		////SARSA+AMAF
		//evaluated->UCT_param_C = -8.948;
		//evaluated->AMAF_param_alpha = 1.137;
		//evaluated->QVALUE_param_scoreWeight = 0.776;
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 1.0004;
		//evaluated->QVALUE_param_lambda = 0.6664;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		////-- Gomoku 19x19 iterations 1000-1000 --//
		////Plain UCT
		//evaluated->UCT_param_C = 0.046;
		////Plain UCT optimized 4000-1000
		//evaluated->UCT_param_C = 0.037;

		////RAVE
		//evaluated->UCT_param_C = 0.997;
		//evaluated->RAVE_param_V = 111;
		////RAVE optimized 4000-1000
		//evaluated->UCT_param_C = -0.083;
		//evaluated->RAVE_param_V = 195;
		////RAVE optimized 8000-2000
		//evaluated->UCT_param_C = 3.452;
		//evaluated->RAVE_param_V = 46.9;

		////SARSA
		//evaluated->UCT_param_C = 1.158;		
		//evaluated->QVALUE_param_scoreWeight = 0.915;	
		//evaluated->QVALUE_param_alpha = 0.7209;
		//evaluated->QVALUE_param_gamma = 0.9072;
		//evaluated->QVALUE_param_lambda = 0.6071;
		//evaluated->QVALUE_param_lambda_playOut = 0.85812;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		////SARSA+AMAF
		//evaluated->UCT_param_C = -3.802;
		//evaluated->AMAF_param_alpha = 0.863;
		//evaluated->QVALUE_param_scoreWeight = 2.851;
		//evaluated->QVALUE_param_alpha = 0.3124;
		//evaluated->QVALUE_param_gamma = 1.0004;
		//evaluated->QVALUE_param_lambda = 0.6664;
		//evaluated->QVALUE_param_lambda_playOut = 0.9129;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		//-- TicTacToe 100 iterations --//

		////SARSA+AMAF
		//evaluated->UCT_param_C = 0.1705;
		////evaluated->AMAF_param_alpha = 0.22765;
		//evaluated->QVALUE_param_scoreWeight = 1.0;
		//evaluated->QVALUE_param_alpha = 0.3685;
		//evaluated->QVALUE_param_gamma = 1.000;
		//evaluated->QVALUE_param_lambda = 0.36015;
		//evaluated->QVALUE_param_lambda_playOut = 0.84016;
		////evaluated->QVALUE_param_leaf_node_val0 = 0;
		////evaluated->QVALUE_param_leaf_node_val1 = 1;
		////evaluated->QVALUE_param_leaf_node_val2 = 0;
		////evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		// -- Hex 7x7 iterations 100-100 --//
		////Plain UCT

		////RAVE
		//evaluated->UCT_param_C = -0.12;
		//evaluated->RAVE_param_V = 158;

		////SARSA

		////SARSA+AMAF
		//evaluated->UCT_param_C = -2.5641;
		//evaluated->AMAF_param_alpha = 0.70345;
		//evaluated->QVALUE_param_scoreWeight = 1.265;
		//evaluated->QVALUE_param_alpha = 0.085;
		//evaluated->QVALUE_param_gamma = 1.000;
		//evaluated->QVALUE_param_lambda = 1.1717;
		//evaluated->QVALUE_param_lambda_playOut = 0.90422;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		// -- Hex 7x7 iterations 1000-1000 --//

		////SARSA+AMAF
		//evaluated->UCT_param_C = -6.6244;
		//evaluated->AMAF_param_alpha = 0.7883;
		//evaluated->QVALUE_param_scoreWeight = 0.3242;
		//evaluated->QVALUE_param_alpha = 0.1563;
		//evaluated->QVALUE_param_gamma = 1.000;
		//evaluated->QVALUE_param_lambda = 0.63945;
		//evaluated->QVALUE_param_lambda_playOut = 0.91768;
		//evaluated->QVALUE_param_leaf_node_val0 = 0;
		//evaluated->QVALUE_param_leaf_node_val1 = 1;
		//evaluated->QVALUE_param_leaf_node_val2 = 0;
		//evaluated->QVALUE_param_leaf_node_val3 = 1;
		//evaluated->QVALUE_param_initVal = 0.5;

		//-- 2014_02_21 Gomo Qval NN search

		////2014_02_21 Gomo Qval single_NN search - initQ
		//funcApp1->parameter_weights[0] = -0.95;
		//funcApp1->parameter_weights[1] = 1.9;
		//funcApp1->parameter_weights[2] = 0.17;
		//funcApp1->parameter_weights[3] = -0.03;

		////2014_02_21 Gomo Qval single_NN search - leaf_val - 1 - long
		//funcApp1->parameter_weights[0] = -0.77;
		//funcApp1->parameter_weights[1] = 3.84;
		//funcApp1->parameter_weights[2] = -2.24;
		//funcApp1->parameter_weights[3] = -0.08;

		//-- end: 2014_02_21 Gomo Qval NN search

		//2014_01_08 RAVE LRP direct search + single NN - first experiments
		//Conn4 singleN RAVE0 bias + test8.1 + test1.1
		//single-layer NN score and weights
		//best solution
		//46.9200    0.0019    0.4079   -0.1863   -0.5264   -0.2975   -1.1809
		//funcApp1->parameter_weights[0] = 0.0019;
		//funcApp1->parameter_weights[1] = 0.4079;
		//funcApp1->parameter_weights[2] = -0.1863;
		//funcApp1->parameter_weights[3] = -0.5264;
		//funcApp1->parameter_weights[4] = -0.2975;
		//funcApp1->parameter_weights[5] = -1.1809;

		//2014_01_10 RAVE LRP dual layer NN - first exp, week run
		//Conn4 - merged
		//dual-layer NN score and weights
		//best solution
		//48.3800    0.0667   -0.8963   -1.2155   -3.7149   -3.5235   -0.8082   -4.7469    3.1732  
		//-1.5695   -2.4915 -0.6675    1.6720    0.8763    1.1854    1.8081   -0.4054   -2.1228   -0.6290    1.2969    1.0485
		//1M evaluations score: 48.83 +-0.1

		//funcApp1->parameter_weights[0] = 0.0667;
		//funcApp1->parameter_weights[1] = -0.8963;
		//funcApp1->parameter_weights[2] = -1.2155;
		//funcApp1->parameter_weights[3] = -3.7149;
		//funcApp1->parameter_weights[4] = -3.5235;
		//funcApp1->parameter_weights[5] = -0.8082;
		//funcApp1->parameter_weights[6] = -4.7469;
		//funcApp1->parameter_weights[7] = 3.1732;
		//funcApp1->parameter_weights[8] = -1.5695;
		//funcApp1->parameter_weights[9] = -2.4915;
		//funcApp1->parameter_weights[10] = -0.6675;
		//funcApp1->parameter_weights[11] = 1.6720;
		//funcApp1->parameter_weights[12] = 0.8763;
		//funcApp1->parameter_weights[13] = 1.1854;
		//funcApp1->parameter_weights[14] = 1.8081;
		//funcApp1->parameter_weights[15] = -0.4054;
		//funcApp1->parameter_weights[16] = -2.1228;
		//funcApp1->parameter_weights[17] = -0.6290;
		//funcApp1->parameter_weights[18] = 1.2969;
		//funcApp1->parameter_weights[19] = 1.0485;

		//2014_01 Q-Values test: analyzed best param values for SARSA(lambda)
		//see weights settings in "2014_01 Q-Values test\sarsa-lambda 11 weight mapping to parameters.txt"
		//funcApp1->parameter_weights[0] = 2.5;
		//funcApp1->parameter_weights[1] = 1.0;
		//funcApp1->parameter_weights[2] = -0.5;
		//funcApp1->parameter_weights[3] = 2.5;
		//funcApp1->parameter_weights[4] = 0.0;
		//funcApp1->parameter_weights[5] = 0.0;
		//funcApp1->parameter_weights[6] = 0.0;
		//funcApp1->parameter_weights[7] = 0.0;
		//funcApp1->parameter_weights[8] = 0.0;
		//funcApp1->parameter_weights[9] = 0.0;
		//funcApp1->parameter_weights[10] = 0.0;

		//funcApp1->parameter_weights[0] = 0;
		//funcApp1->parameter_weights[1] = 1;
		//funcApp1->parameter_weights[2] = 0;
		//funcApp1->parameter_weights[3] = 1;

		////2014_03_07 SARSA+AMAF_EQ3 single NN by sum_iter
		//funcApp1->parameter_weights[0] = -0.8054;
		//funcApp1->parameter_weights[1] = 0.7459;
		//funcApp1->parameter_weights[2] = -1.3799;
		//funcApp1->parameter_weights[3] = -0.2492;
		//funcApp1->parameter_weights[4] = 0.9302;
		//funcApp1->parameter_weights[5] = 0.2021;
		//funcApp1->parameter_weights[6] = 0.2007;
		//funcApp1->parameter_weights[7] = 0.303;
		//funcApp1->parameter_weights[8] = 0.0062;

		//---execute testing---//

		benchmarkOpponent->UCT_param_IterNum = opponent->UCT_param_IterNum;
		benchmarkOpponent->UCT_param_C = opponent->UCT_param_C;
		benchmarkEvaluated->UCT_param_IterNum = evaluated->UCT_param_IterNum;
		benchmarkEvaluated->UCT_param_C = evaluated->UCT_param_C;
		benchmarkEvaluated->AMAF_param_alpha = evaluated->AMAF_param_alpha;
		benchmarkEvaluated->RAVE_param_V = evaluated->RAVE_param_V;
		//TODO copy all new Qval params to benchmarkEvaluated

		players[1 - eval_player_position] = opponent;
		players[eval_player_position] = evaluated;

		//setup result storage
		Tom_Sample_Storage<double>* score[2];

		gmp->Print("Fixed_Play_Testing()\n");
		gmp->Print("%s repeats %d (input params %3.3lf %3.3lf)\n", (game->game_name).c_str(), repeats, input_param1, input_param2);
		gmp->Print("Evaluated player %d sim %d\n", eval_player_position + 1, evaluated->UCT_param_IterNum);


		if ((display_output_game == 0) && (human_opponent_override == 0)){

			gmp->Print("Opponent C %3.3f, sim %d\n", opponent->UCT_param_C, opponent->UCT_param_IterNum);
			gmp->Print("\n");
			gmp->Print("        Score[%%]\n");
			gmp->Print(" Repeats     Avg  Conf95     Dev Runtime[s]\n");

			score[0] = new Tom_Sample_Storage<double>(repeats);
			score[1] = new Tom_Sample_Storage<double>(repeats);

			double cpu_time = getCPUTime();

			if (benchmark_same_player == 0){
				game->Evaluate_Players(1, repeats, -1, players, false, eval_player_position, score, output_interval_repeats, measure_time_per_move);
			}
			else if (benchmark_same_player == 1){
				gmp->Print("! benchmark_same_player = opponent vs opponent !\n");
				players[eval_player_position] = benchmarkOpponent;
				game->Evaluate_Players(1, repeats, -1, players, false, eval_player_position, score, output_interval_repeats, measure_time_per_move);
			}
			else if (benchmark_same_player == 2){
				gmp->Print("! benchmark_same_player = evaluated vs evaluated !\n");
				players[1 - eval_player_position] = benchmarkEvaluated;
				game->Evaluate_Players(1, repeats, -1, players, false, eval_player_position, score, output_interval_repeats, measure_time_per_move);
			}

			cpu_time = getCPUTime() - cpu_time;

			gmp->Print("%8d  %6.2f  %6.2f  %6.2f  %9.3f\n",
				repeats,
				score[eval_player_position]->avg * 100,
				score[eval_player_position]->Calc_Confidence() * 100,
				score[eval_player_position]->dev * 100,
				cpu_time
				);

			delete(score[0]);
			delete(score[1]);

		}
		else{

			gmp->Print("! HUMAN Opponent !\n");
			gmp->Print("\n");
			if (human_opponent_override == 1)
				players[1 - eval_player_position] = new Player_Human(game);
			game->Simulate_Output_Game(players);
			if (human_opponent_override == 1)
				delete(players[1 - eval_player_position]);
		}

		gmp->Flush();

		//gmp->Print("\n\nFixed_Play_Testing(): TotalRuntime: %9.3f s\n", cpu_time);
		delete(game);
		delete(opponent);
		delete(benchmarkOpponent);
		delete(evaluated);
		delete(benchmarkEvaluated);
	}

	void Tom_Paper1_tests()
	{
		////F1,F23
		//Fixed_Play_Testing(10);
		//Fixed_Play_Testing(20);
		//Fixed_Play_Testing(50);
		//Fixed_Play_Testing(200);
		//Fixed_Play_Testing(500);
		//Fixed_Play_Testing(1000);

		////F4
		//Fixed_Play_Testing(7);
		//Fixed_Play_Testing(9);
		//Fixed_Play_Testing(11);
		//Fixed_Play_Testing(13);
		//Fixed_Play_Testing(15);
		//Fixed_Play_Testing(17);
		//Fixed_Play_Testing(19);

		////Hex/TTT test
		//Fixed_Play_Testing(10,10);
		//Fixed_Play_Testing(20,20);
		//Fixed_Play_Testing(30,30);
		//Fixed_Play_Testing(40,40);
		//Fixed_Play_Testing(10,20);
		//Fixed_Play_Testing(10,30);
		//Fixed_Play_Testing(10,40);
		//Fixed_Play_Testing(10,50);
		//Fixed_Play_Testing(20,10);
		//Fixed_Play_Testing(30,10);
		//Fixed_Play_Testing(40,10);
		//Fixed_Play_Testing(50,10);
		//Fixed_Play_Testing(50,50);
		//Fixed_Play_Testing(50,60);
		//Fixed_Play_Testing(50,70);
		//Fixed_Play_Testing(50,80);
		//Fixed_Play_Testing(50,90);
		//Fixed_Play_Testing(60,50);
		//Fixed_Play_Testing(70,50);
		//Fixed_Play_Testing(80,50);
		//Fixed_Play_Testing(90,50);
		//Fixed_Play_Testing(100,100);

		////Hex test
		//Fixed_Play_Testing(50,50);
		//Fixed_Play_Testing(50,100);
		//Fixed_Play_Testing(100,50);
		//Fixed_Play_Testing(100,100);
		//Fixed_Play_Testing(200,200);
		//Fixed_Play_Testing(200,500);
		//Fixed_Play_Testing(500,200);
		//Fixed_Play_Testing(500,500);
		//Fixed_Play_Testing(500,1000);
		//Fixed_Play_Testing(1000,500);
		//Fixed_Play_Testing(1000,1000);
	}

}