#include "kernel/register.h"
#include "kernel/sigtools.h"
#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/log.h"
#include "kernel/yosys.h"
#include "libs/sha1/sha1.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cctype>
#include <cerrno>
#include <sstream>
#include <climits>
#include <Python/Python.h>

USING_YOSYS_NAMESPACE

PRIVATE_NAMESPACE_BEGIN

/* taking the same approach used in the ABC and lsoracle passes. It's possible we should do it differently, but this is a starting point */
SigMap assign_map;
RTLIL::Module *module;
dict<int, std::string> pi_map, po_map;
std::map<RTLIL::SigBit, int> signal_map;
std::map<RTLIL::SigBit, RTLIL::State> signal_init;

struct python_output_filter
{
	bool got_cr;
	int escape_seq_state;
	std::string linebuf;
	std::string tempdir_name;
	bool show_tempdir;

	python_output_filter(std::string tempdir_name, bool show_tempdir) : tempdir_name(tempdir_name), show_tempdir(show_tempdir)
	{
		got_cr = false;
		escape_seq_state = 0;
	}

	void next_char(char ch)
	{
		if (escape_seq_state == 0 && ch == '\033') {
			escape_seq_state = 1;
			return;
		}
		if (escape_seq_state == 1) {
			escape_seq_state = ch == '[' ? 2 : 0;
			return;
		}
		if (escape_seq_state == 2) {
			if ((ch < '0' || '9' < ch) && ch != ';')
				escape_seq_state = 0;
			return;
		}
		escape_seq_state = 0;
		if (ch == '\r') {
			got_cr = true;
			return;
		}
		if (ch == '\n') {
			got_cr = false, linebuf.clear();
			return;
		}
		if (got_cr)
			got_cr = false, linebuf.clear();
		linebuf += ch;
	}
    
    void next_line(const std::string &line)
	{
		int pi, po;
		if (sscanf(line.c_str(), "Start-point = pi%d.  End-point = po%d.", &pi, &po) == 2) {
			log("synth_opt_adders: Start-point = pi%d (%s).  End-point = po%d (%s).\n",
					pi, pi_map.count(pi) ? pi_map.at(pi).c_str() : "???",
					po, po_map.count(po) ? po_map.at(po).c_str() : "???");
			return;
		}

		for (char ch : line)
			next_char(ch);
	}
};

void python_caller(RTLIL::Design *design, int width, std::string start, std::string transform, std::string mapping, std::string hdl_root){
    std::string tempdir_name = "/tmp/synth-adder-XXXXXX";
	tempdir_name = make_temp_dir(tempdir_name);   
    python_output_filter filt(tempdir_name, false);
	char pwd [PATH_MAX];
	if (!getcwd(pwd, sizeof(pwd))) {
		log_cmd_error("getcwd failed: %s\n", strerror(errno));
		log_abort();
	}
	std::string script_path = std::string(pwd)+ "/../util/gen_adder.py";
	Py_Initialize();
	FILE* gen_adder_script = fopen(script_path.c_str(), "r"); //still figuring out if this is how I want to do this
	
	if (gen_adder_script){
		PyRun_SimpleFile(gen_adder_script, "gen_adder.py");
		fclose(gen_adder_script);
	}

	Py_Finalize();

    /*std::string py_command = "PLACEHOLDER";
	int ret = run_command(py_command, std::bind(&python_output_filter::next_line, filt, std::placeholders::_1));
	if (ret != 0)
		log_error("Synth_opt_adder: execution of command \"%s\" failed: return code %d.\n", py_command.c_str(), ret);
	*/
}

struct OptAdders : public Pass {
	OptAdders() : Pass("optadders", "Optimize adders using synth_opt_adders tool") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    optadders [options] [selection]\n");
		log("\n");
		log("This pass uses the synth_opt_adders tool to optimize adders\n");
		log("\n");
		log("    -width <int>\n");
		log("        adder width (default: 32).\n");
        log("    -start <string>\n");
		log("        starting architecture (default: ripple carry)).\n");
        log("    -transforms <string>\n");
		log("       transforms to apply (default: none. example: _LF@6_LF@4).\n");
        log("    -mapping <string>\n");
		log("       mapping strategy (default: behavioral).\n");
        log("    -hdl_root <string>\n");
		log("       (default: none).\n");       
		log("\n");

	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing optadders pass.\n");
		log_push();

        int width = 32;
        std::string start = "ripple";
        std::string transform = "";
        std::string mapping = "behavioral";
        std::string hdl_root = "";

        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++) {
            std::string arg = args[argidx];
            if (arg == "-width" && argidx+1 < args.size()) {
				std::string s_width = args[++argidx];
                width = std::stoi(s_width);
				continue;
			}
            if (arg == "-start" && argidx+1 < args.size()) {
				start = args[++argidx]; //super fragile; do this better
				continue;
			}
            if (arg == "-transform" && argidx+1 < args.size()) {
				transform = args[++argidx]; //super fragile; do this better
				continue;
			}
            if (arg == "-mapping" && argidx+1 < args.size()) {
				mapping = args[++argidx]; //super fragile; do this better
				continue;
			}
            if (arg == "-hdl_root" && argidx+1 < args.size()) {
				hdl_root = args[++argidx]; //super fragile; do this better
				continue;
			}
        }
        
        extra_args(args, argidx, design);

		for (auto mod : design->selected_modules())
			{
				if (mod->processes.size() > 0) {
					log("Skipping module %s as it contains processes.\n", log_id(mod));
					continue;
				}

				assign_map.set(mod);
				signal_init.clear();

				for (Wire *wire : mod->wires()){
					if (wire->attributes.count("\\init")) {
						SigSpec initsig = assign_map(wire);
						Const initval = wire->attributes.at("\\init");
						for (int i = 0; i < GetSize(initsig) && i < GetSize(initval); i++)
							switch (initval[i]) {
								case State::S0:
									signal_init[initsig[i]] = State::S0;
									break;
								case State::S1:
									signal_init[initsig[i]] = State::S1;
									break;
								default:
									break;
							}
					}
        			
					python_caller(design, width, start, transform, mapping, hdl_root);
					continue;
				}
			}
    }
} OptAdders;

PRIVATE_NAMESPACE_END
