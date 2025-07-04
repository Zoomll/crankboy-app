import sys
import json

def load_pdxinfo(filename):
    data = {}
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line and '=' in line:
                key, value = line.split('=', 1)
                data[key] = value
    return data

def write_pdxinfo(data, output_file=None):
    content = '\n'.join(f"{k}={v}" for k, v in data.items())
    if output_file:
        with open(output_file, 'w') as f:
            f.write(content + '\n')
    else:
        print(content)

def main():
    if len(sys.argv) < 3:
        print("Usage: python script.py <pdxinfo_path> <version_json_path> [output_path]")
        sys.exit(1)
    
    pdxinfo_path = sys.argv[1]
    version_json_path = sys.argv[2]
    output_path = sys.argv[3] if len(sys.argv) > 3 else None
    
    pdxinfo = load_pdxinfo(pdxinfo_path)
    with open(version_json_path, 'r') as f:
        version_json = json.load(f)
    
    # Extract version (remove leading 'v' if present)
    new_version = version_json['name'].lstrip('v')
    
    pdxinfo['version'] = new_version
    
    write_pdxinfo(pdxinfo, output_path)

if __name__ == "__main__":
    main()