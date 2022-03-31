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
//#include <Python.h>

USING_YOSYS_NAMESPACE

PRIVATE_NAMESPACE_BEGIN

// ### START OF CODE related to Yosys #3242 ###

// Find neighbor $add cells related to carry-in
RTLIL::Cell* find_neighbor_add(RTLIL::Cell *cell, RTLIL::Module *module)
{
	// Get output wire of $add
	SigSpec sig_a = cell->getPort("\\Y");
	// Search for another $add that connects to the output wire
	for (RTLIL::Cell *c : module->selected_cells())
	{
		if (c->type == "$add" && c->getPort("\\A") == sig_a && GetSize(c->getPort("\\B")) == 1)
			return c;
	}
	return cell;
}

// Save attributes of $add cells before they are removed by alumacc
dict <std::string, dict<RTLIL::IdString, RTLIL::Const>> save_attributes(RTLIL::Design *design)
{
	dict<std::string, dict<RTLIL::IdString, RTLIL::Const>> saved_attributes;
	for (RTLIL::Module *module : design->selected_modules()){
		for (RTLIL::Cell *cell : module->selected_cells()) {
			// Only check $add cells with the pptrees_alu attribute
			if (cell->type.in(ID($add),ID($sub)) && cell->attributes.count("\\pptrees_alu")) {
				// Check to see whether there's a neighbor that will be part of the same $alu
				std::string cell_src = find_neighbor_add(cell,module)->get_src_attribute();
				// Associate the cell's attributes with its "src" attribute
				saved_attributes[cell_src] = cell->attributes;
			}
		}
	}
	return saved_attributes;
}

// ### END OF CODE related to Yosys #3242 ###

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
		for (char ch : line)
			next_char(ch);
	}
};

// Call on the pptrees library to output mapping files
// Return the temporary directory name
std::string python_maps(std::string mapping){

	// Create temporary directory for the mapping files
	std::string tempdir_name = "/tmp/yosys-python-XXXXXX";
	tempdir_name = make_temp_dir(tempdir_name);
	python_output_filter filt(tempdir_name, false);
	
	// Define python executable
	std::string python_executable = "python3";
	
	// Define mapping output script locally, to avoid dealing with an extra file
	// Import modules
	std::string py_command = "import sys; from pptrees.yosys_alu import yosys_alu as tree; ";
	// Initialize a default tree
	py_command.append(stringf("g=tree(1); "));
	// Write out mappings
	py_command.append(stringf("g.yosys_map('%s','%s');", tempdir_name.c_str(), mapping.c_str()));
	
	// Call python
	std::string shell_command = stringf("%s -c \"%s\"", python_executable.c_str(), py_command.c_str());

	log("Running python script from shell: %s\n", shell_command.c_str());
	int ret = run_command(shell_command, std::bind(&python_output_filter::next_line, filt, std::placeholders::_1));

	if (ret != 0)
		log_error("Synth_opt_adder: execution of command \"%s\" failed: return code %d.\n", py_command.c_str(), ret);

	return tempdir_name;
}

// Call on the pptrees library to generate a tree
// Return the temporary directory name
std::string python_tree(int width, std::string start, std::string transform, std::string mapping){

	// Create temporary directory for python output
	std::string tempdir_name = "/tmp/yosys-pptrees-XXXXXX";
	tempdir_name = make_temp_dir(tempdir_name);
	python_output_filter filt(tempdir_name, false);

	// Retrieve the randomly-generated directory name, and use as the module name
	std::string adder_name = tempdir_name.substr(19);

	// Define python executable
	std::string python_executable = "python3";

	// Define adder generation script locally, to avoid dealing with an extra file
	// Import modules
	std::string py_command = "import sys; from pptrees.yosys_alu import yosys_alu as tree; ";
	// Initialize the tree
	py_command.append(stringf("g=tree(%d,'%s'); ", width, start.c_str()));
	// Apply each transform
	py_command.append("[getattr(g,t.split('@')[0])(*[int(x) for x in t.split('@')[1].split(',')]) ");
	// Loop over the transforms
	py_command.append(stringf("for t in '%s'.split('_')[1:]];", transform.c_str()));
	// Output the HDL
	py_command.append(stringf("g.hdl('%s/pptrees_alu.v','%s',top_module='_%s_adder');",tempdir_name.c_str(),mapping.c_str(),adder_name.c_str()));

	// Call python
	std::string shell_command = stringf("%s -c \"%s\"", python_executable.c_str(), py_command.c_str());

	log("Running python script from shell: %s\n", shell_command.c_str());
	int ret = run_command(shell_command, std::bind(&python_output_filter::next_line, filt, std::placeholders::_1));

	if (ret != 0)
		log_error("Synth_opt_adder: execution of command \"%s\" failed: return code %d.\n", py_command.c_str(), ret);

	return tempdir_name;
}

struct opt_pptrees : public Pass {
	opt_pptrees() : Pass("opt_pptrees", "Optimize parallel prefix trees using pptrees Python package") { }
	
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("	opt_pptrees [options] [selection]\n");
		log("\n");
		log("This pass uses the synth_opt_adders tool to optimize adders\n");
		log("\n");
		log("	-mapping <string>\n");
		log("		mapping strategy (default: behavioral).\n");
		log("\n");

	}

	void execute(std::vector<std::string> args, RTLIL::Design *design) override
	{
		log_header(design, "Executing opt_pptrees pass.\n");
		log_push();

        int width;
        std::string start = "ripple-carry";
        std::string transforms = "";
        std::string mapping = "behavioral";

	// Parse options
        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++) {
            std::string arg = args[argidx];
		if (arg == "-mapping" && argidx+1 < args.size()) {
			mapping = args[++argidx]; //super fragile; do this better
		}
        }
        
        extra_args(args, argidx, design);

	// Write out mappping files
	std::string mapping_dir = python_maps(mapping);

	// Get the output of the python script
	Pass::call(design, stringf("read_verilog -sv %s/%s_map.v", mapping_dir.c_str(), mapping.c_str()));
	Pass::call(design, stringf("read_verilog -sv %s/modules.v", mapping_dir.c_str()));

	// Save attributes of $add cells before they are removed by alumacc
	dict<std::string, dict<RTLIL::IdString, RTLIL::Const>> saved_attributes = save_attributes(design);

	// Run alumacc pass
	Pass::call(design, "alumacc");

	// Iterate through all selected modules
	for (RTLIL::Module *mod : design->selected_modules())
	{
		// Skip modules that contain processes
		if (mod->has_processes_warn() ) {
			log("Skipping module %s as it contains processes.\n", log_id(mod));
			continue;
		}


		// Iterate through all the cells in the module
		for (RTLIL::Cell *cell : mod->selected_cells()) {

			// Find $alu cells whose attributes were saved
			if (cell->type == ID($alu) && saved_attributes.count(cell->get_src_attribute())) {

				// Restore attributes
				cell->attributes = saved_attributes[cell->get_src_attribute()];

				// Get the width of the adder
				width = cell->parameters.at(ID::A_WIDTH).as_int();
				if (cell->parameters.at(ID::B_WIDTH).as_int() > width)
					width = cell->parameters.at(ID::B_WIDTH).as_int();
				
				// Get the starting architecture
				if (cell->attributes.count("\\pptrees_base")) {
					start = cell->attributes.at("\\pptrees_base").decode_string();
				}
				
				// Get the transforms to apply
				std::string transforms;
				if (cell->attributes.count("\\pptrees_transforms")) {
					transforms = cell->attributes.at("\\pptrees_transforms").decode_string();
				}

				// Call the python script
				std::string pptrees_dir = python_tree(width, start, transforms, mapping);

				// Get the output of the python script
				Pass::call(design, stringf("read_verilog -sv %s/pptrees_alu.v",pptrees_dir.c_str()));

				// Select and techmap this $alu cell
				Pass::call(design, stringf("select a:src=%s",cell->get_src_attribute().c_str()));
				Pass::call(design, "techmap -map pptrees_alu.v %");

			}
                }
	}
    }

} opt_pptrees;

PRIVATE_NAMESPACE_END
