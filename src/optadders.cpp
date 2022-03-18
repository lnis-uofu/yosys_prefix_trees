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

USING_YOSYS_NAMESPACE

PRIVATE_NAMESPACE_BEGIN


std::string prepend_script_file(std::string script_file, std::string input_file, std::string output_file)
{
	std::ifstream script;
	script.open(script_file);
	std::stringstream lso_script;
	lso_script << stringf("read %s; ", input_file.c_str());
	lso_script << script.rdbuf();
	lso_script << stringf("write_blif %s", output_file.c_str());
	return lso_script.str();
}

std::string generate_lso_script(std::string exe_file, std::string input_aig_file, std::string output_blif_file,
			std::string num_parts, bool partitioned, bool exclu_part, bool mig,
			bool deep, bool merge, bool test, bool aig, bool lut)
{

	std::string lso_script;

	std::string config_direc = exe_file;
	config_direc.erase(config_direc.begin() + config_direc.find("lsoracle"), config_direc.end());

	if(!partitioned)
		lso_script += mig ? stringf("read -m %s; ", input_aig_file.c_str()) : stringf("read %s; ", input_aig_file.c_str());
	else
		lso_script += stringf("read %s; ", input_aig_file.c_str());

	//Conversion from RTLIL to AIG readable by LSOracle
	if(!test){
		if(partitioned){
			if(exclu_part)
				lso_script += mig ? LSO_COMMAND_PART_EXCLU_MIG : LSO_COMMAND_PART_EXCLU_AIG;
			else{
				if(merge)
					lso_script += deep ? LSO_COMMAND_PART_DEEP_M : LSO_COMMAND_PART_HIGH_EFFORT_M;
				else
					lso_script += deep ? LSO_COMMAND_PART_DEEP : LSO_COMMAND_PART_HIGH_EFFORT;
			}
		}
		else{
			// lso_script += mig ? LSO_COMMAND_MIG : LSO_COMMAND_AIG;
			if(mig)
				lso_script += LSO_COMMAND_MIG;
			else if(aig)
				lso_script += LSO_COMMAND_AIG;
			else{
				if(merge)
					lso_script += LSO_COMMAND_PART_HIGH_EFFORT_M;
				else
					lso_script += LSO_COMMAND_PART_HIGH_EFFORT;
			}

		}

	}

	if(test){
		lso_script += !lut ? stringf("write_blif %s", output_blif_file.c_str()) : stringf("lut_map -o %s", output_blif_file.c_str());
	}
	else{
		if(!partitioned){
			if(aig)
				lso_script += !lut ? stringf("; write_blif %s", output_blif_file.c_str()) : stringf("; lut_map -o %s", output_blif_file.c_str());
			else
				lso_script += !lut ? stringf("; write_blif -m %s", output_blif_file.c_str()) : stringf("; lut_map -m -o %s", output_blif_file.c_str());
		}
		else{
			if(aig)
				lso_script += !lut ? stringf("; write_blif %s", output_blif_file.c_str()) : stringf("; lut_map -o %s", output_blif_file.c_str());
			else
				lso_script += !lut ? stringf("; write_blif -m %s", output_blif_file.c_str()) : stringf("; lut_map -m -o %s", output_blif_file.c_str());
		}
	}

	for (size_t pos = lso_script.find("{P}"); pos != std::string::npos; pos = lso_script.find("{P}", pos))
		lso_script = lso_script.substr(0, pos) + num_parts + " -c " + config_direc + "../../core/test.ini" + lso_script.substr(pos+3);

	for (size_t pos = lso_script.find("{C}"); pos != std::string::npos; pos = lso_script.find("{C}", pos))
		lso_script = lso_script.substr(0, pos) + config_direc + "../../core/test.ini" + lso_script.substr(pos+3);

	for (size_t pos = lso_script.find("{D}"); pos != std::string::npos; pos = lso_script.find("{D}", pos))
		lso_script = lso_script.substr(0, pos) + config_direc + "../../deep_learn_model.json" + lso_script.substr(pos+3);
	return lso_script;
}

std::string write_lso_script(std::string lso_script, std::string tempdir_name )
{
	log("LSOracle script: %s\n", lso_script.c_str());
	std::string filename = stringf("%s/lso.script", tempdir_name.c_str());
	FILE *f = fopen(filename.c_str(), "wt");
	fprintf(f, "%s\n", lso_script.c_str());
	fclose(f);
	return filename;
}

void lso_module(std::string exe_file, std::string tempdir_name, bool show_tempdir, std::string filename)
{
	std::string buffer = stringf("%s -f %s 2>&1", exe_file.c_str(), filename.c_str());
	log("Running LSOracle command: %s\n", replace_tempdir(buffer, tempdir_name, show_tempdir).c_str());
	lso_output_filter filt(tempdir_name, show_tempdir);
	int ret = run_command(buffer, std::bind(&lso_output_filter::next_line, filt, std::placeholders::_1));

	if (ret != 0)
		log_error("LSOracle: execution of command \"%s\" failed: return code %d.\n", buffer.c_str(), ret);
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
		log("    -script <file>\n");
		log("        use the specified script file instead of the default script.\n");
		log("\n");

	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing optadders pass.\n");
		log_push();
    }
} OptAdders;

PRIVATE_NAMESPACE_END
