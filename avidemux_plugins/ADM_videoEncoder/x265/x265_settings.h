// automatically generated by admSerialization.py do not edit
#include "string"
#pragma once
typedef struct {
bool useAdvancedConfiguration;
struct  {
	COMPRES_PARAMS params;
	uint32_t poolThreads;
	uint32_t frameThreads;
	std::string preset;
	std::string tuning;
	std::string profile;
}general;
int32_t level;
struct  {
	uint32_t sar_height;
	uint32_t sar_width;
}vui;
uint32_t MaxRefFrames;
uint32_t MinIdr;
uint32_t MaxIdr;
uint32_t i_scenecut_threshold;
uint32_t MaxBFrame;
uint32_t i_bframe_adaptive;
uint32_t i_bframe_bias;
uint32_t i_bframe_pyramid;
bool b_deblocking_filter;
bool b_open_gop;
uint32_t interlaced_mode;
bool constrained_intra;
bool b_intra;
uint32_t lookahead;
uint32_t weighted_pred;
bool weighted_bipred;
bool rect_inter;
bool amp_inter;
bool limit_modes;
uint32_t cb_chroma_offset;
uint32_t cr_chroma_offset;
uint32_t me_method;
uint32_t me_range;
uint32_t subpel_refine;
uint32_t limit_refs;
uint32_t rd_level;
double psy_rd;
uint32_t rdoq_level;
double psy_rdoq;
bool fast_pskip;
bool dct_decimate;
uint32_t noise_reduction_intra;
uint32_t noise_reduction_inter;
bool strong_intra_smoothing;
struct  {
	uint32_t rc_method;
	uint32_t qp_constant;
	uint32_t qp_step;
	uint32_t bitrate;
	uint32_t vbv_max_bitrate;
	uint32_t vbv_buffer_size;
	uint32_t vbv_buffer_init;
	double ip_factor;
	double pb_factor;
	uint32_t aq_mode;
	double aq_strength;
	bool cu_tree;
	bool strict_cbr;
}ratecontrol;
}x265_settings;
