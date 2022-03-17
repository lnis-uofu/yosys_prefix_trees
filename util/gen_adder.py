from pptrees.adder_tree import adder_tree as tree
from os.path import join
import argparse

def parse_args():
    """Import command-line arguments"""
    argparser = argparse.ArgumentParser()
    argparser.add_argument("--width",type=int,default=32)
    argparser.add_argument("--start",type=str,default="ripple-carry")
    argparser.add_argument("--transforms",type=str,default="",
            help="example: _LF@6_LF@4")
    argparser.add_argument("--top_module",type=str,default="adder")
    argparser.add_argument("--mapping",type=str,default="behavioral")
    argparser.add_argument("--hdl_root",type=str,default=".")

    return argparser.parse_args()

def main():
    """Generate HDL"""
    args = parse_args()

    # Output file name may have to change
    output_hdl = join(args.hdl_root,"{0}.v".format(args.top_module))

    # Language should always be verilog, but who knows what can happen
    hdl_lang = "verilog"

    g = tree(args.width,args.start)
    for a in args.transforms.split("_")[1:]:
        getattr(g,a.split("@")[0])(*[int(x) for x in a.split("@")[1].split(",")]);

    g.hdl(out=output_hdl,mapping=args.mapping,language=hdl_lang,top_module=args.top_module)
    g.png('test.png')

if __name__=="__main__":
    main()
