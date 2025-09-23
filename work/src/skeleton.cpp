
// std
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>

// project
#include "opengl.hpp"
#include "skeleton.hpp"


using namespace std;
using namespace glm;



// helper method for retreiving and trimming the next line in a file.
namespace {
	string nextLineTrimmed(istream &file) {
		string line;
		getline(file, line);
		// remove leading and trailing whitespace and comments
		size_t i = line.find_first_not_of(" \t\r\n");
		if (i != string::npos) {
			if (line[i] != '#') {
				return line.substr(i, line.find_last_not_of(" \t\r\n") - i + 1);
			}
		}
		return "";
	}
}



int skeleton_data::findBone(const std::string &name) const {
	for (size_t i = 0; i < bones.size(); i++)
		if (bones[i].name == name)
			return i;
	return -1;
}



skeleton_data::skeleton_data(const std::string &filename) {

	ifstream file(filename);

	if (!file.is_open()) {
		cerr << "Failed to open file " <<  filename << endl;
		throw runtime_error("Error :: could not open file.");
	}

	cout << "Reading ASF file" << filename << endl;

	// good() means that failbit, badbit and eofbit are all not set
	while (file.good()) {

		// Pull out line from file
		string line = nextLineTrimmed(file);

		// Check if it is a comment or just empty
		if (line.empty() || line[0] == '#')
			continue;
		else if (line[0] == ':') {
			// Line starts with a ':' character so it must be a header
			readASFHeader(line, file);
		} else {
			// Would normally error here, but becuase we don't parse
			// every header entirely, so we leave this blank.
		}
	}

	cout << "Completed reading ASF file" << endl;
}



void skeleton_data::readASFHeader(const string &headerline, ifstream &file) {

	string head;
	istringstream lineStream(headerline);
	lineStream >> head; // get the first token from the stream

	// remove the ':' from the header name
	if (head[0] == ':')
		head = head.substr(1);

	if (lineStream.fail() || head.empty()) {
		cerr << "Could not get heading name from\"" << headerline << "\", all is lost" << endl;
		throw runtime_error("Error :: could not parse .asf file.");
	}

	if (head == "version") {
		//version string - must be 1.10
		string version;
		lineStream >> version;
		if (lineStream.fail() || version != "1.10") {
			cerr << "Invalid version: \"" << version << "\" must be 1.10" << endl;
			throw runtime_error("Error :: invalid .asf version.");
		}
	}
	else if (head == "name") {
		// This allows the skeleton to be called something
		// other than the file name. We don't actually care
		// what the name is, so can ignore this whole section
	}
	else if (head == "documentation") {
		// Documentation section has no meaningful information
		// only of use if you want to copy the file. So we skip it
	}
	else if (head == "units") {
		// Has factors for the units to be able to model the
		// real person, these must be parsed correctly. Only
		// really need to check if deg or rad, but that is 
		// not needed for this assignment.

		// We are going to assume that the units:length feild
		// is 0.45, and that the angles are in degrees
	}
	else if (head == "root") {
		// Read in information about root. Let's just assume
		// it'll be at the origin for this assignment.
		skeleton_bone root;
		root.name = "root";
		root.freedom |= dof_rx | dof_ry | dof_rz | dof_root;
		bones.push_back(root);
	}
	else if (head == "bonedata") {
		// Read in each bone until we get to the
		// end of the file or a new header
		string line = nextLineTrimmed(file);
		while (file.good() && !line.empty()) {
			if (line[0] == ':') {
				// finished our reading of bones
				// read next header and return
				return readASFHeader(line, file);
			}
			else if (line == "begin") {
				readASFBone(file);
			}
			else {
				cerr << "Expected 'begin' in bone data, found \"" << line << "\"";
				throw runtime_error("Error :: could not parse .asf file.");
			}
			line = nextLineTrimmed(file);
		}
	}
	else if (head == "hierarchy") {
		// Description of how the bones fit together
		// Read in each line until we get to the
		// end of the file or a new header
		string line = nextLineTrimmed(file);
		while (file.good() && !line.empty()) {
			if (line[0] == ':') {
				// finished our reading of bones
				// read next header and return
				return readASFHeader(line, file);
			}
			else if (line == "begin") {
				readHierarchy(file);
			}
			else {
				cerr << "Expected 'begin' in hierarchy, found \"" << line << "\"";
				throw runtime_error("Error :: could not parse .asf file.");
			}
			line = nextLineTrimmed(file);
		}
	}
	else {
		// Would normally error here, but becuase we don't parse
		// every header entirely we will leave this blank.
	}
}



void skeleton_data::readASFBone(ifstream &file) {
	// Create the bone to add the data to
	skeleton_bone bone;

	string line = nextLineTrimmed(file);
	while (file.good()) {
		if (line == "end") {
			// End of the data for this bone
			// Push the bone into the vector
			bones.push_back(bone);
			return;
		}
		else {
			
			string head;
			istringstream lineStream(line);

			// parse the first token
			lineStream >> head; 

			if (head == "name") {
				lineStream >> bone.name;
			}
			else if (head == "direction") {
				lineStream >> bone.direction.x >> bone.direction.y >> bone.direction.z;
				bone.direction = normalize(bone.direction); // normalize here for consistency
			}
			else if (head == "length") {
				lineStream >> bone.length;
				bone.length *= 0.0254 / 0.45;  // scale by 1/0.45 then convert from inches to meters
			}
			else if (head == "dof") {
				// degrees of freedom of the joint (rotation)
				while (lineStream.good()) {
					string dofString;
					lineStream >> dofString;
					if (!dofString.empty()) {
						// parse each dof string
						if      (dofString == "rx") bone.freedom |= dof_rx;
						else if (dofString == "ry") bone.freedom |= dof_ry;
						else if (dofString == "rz") bone.freedom |= dof_rz;
						else throw runtime_error("Error :: could not parse .asf file.");
					}
				}
			}
			else if (head == "axis") {
				// rotation basis
				lineStream >> bone.basis.x >> bone.basis.y >> bone.basis.z;
				bone.basis = radians(bone.basis);
			}
			else if (head == "limits") {
				// limits for each of the DOF assuming the dof has been read first
				// you can optionally fill this method out. it requires reading multiple lines
			}

			// check if we've failed at any point
			if (lineStream.fail()) {
				cerr << "Unable to parse \"" << line << "\"";
				throw runtime_error("Error :: could not parse .asf file.");
			}
		}

		line = nextLineTrimmed(file);
	}

	cerr << "Expected end in bonedata, found \"" << line << "\"";
	throw runtime_error("Error :: could not parse .asf file.");
}



void skeleton_data::readHierarchy(ifstream &file) {
	string line = nextLineTrimmed(file);
	while (file.good()) {
		if (line == "end") {
			// End of hierarchy
			return;
		}
		else if (!line.empty()) {
			// Read the parent node
			string parentName;
			istringstream lineStream(line);
			lineStream >> parentName;

			// Find the parent bone and have a pointer to it
			int parentIndex = findBone(parentName);

			if (parentIndex < 0) {
				cerr << "Expected a valid parent bone name, found \"" << parentName << "\"" << endl;
				throw runtime_error("Error :: could not parse .asf file.");
			}

			//Read the connections
			string childName;
			lineStream >> childName;
			while (!lineStream.fail() && !childName.empty()) {

				int childIndex = findBone(childName);

				if (childIndex < 0) {
					cerr << "Expected a valid child bone name, found \"" << childName << "\"" << endl;
					throw runtime_error("Error :: could not parse .asf file.");
				}
				
				// record child
				bones[parentIndex].children.push_back(childIndex);
				
				// next child
				lineStream >> childName;
			}
		}
		line = nextLineTrimmed(file);
	}
	cerr << "Expected end in bonedata, found \"" << line << "\"";
	throw runtime_error("Error :: could not parse .asf file.");
}



skeleton_animation::skeleton_animation(const string &filename, const skeleton_data &skel) {

	ifstream file(filename);

	if (!file.is_open()) {
		cerr << "Error: Failed to open file " <<  filename << endl;
		throw runtime_error("Error: Failed to open file " + filename);
	}

	cout << "Reading AMC data from " << filename << endl;

	// good() means that failbit, badbit and eofbit are all not set
	while (file.good()) {

		string line = nextLineTrimmed(file);

		// check if it is a comment or a header (which we ignore) or is just empty
		if (line.empty() || line[0] == '#' || line[0] == ':')
			continue;

		// line starts with a digit so it must be the frame number
		else if (isdigit(line[0])) {
			
			istringstream lineStream(line);
			size_t f;
			lineStream >> f;

			// check that the frame number is parsed correctly and is the right number
			if (lineStream.fail()) {
				cout << "Expected frame number, got : " << line << endl; 
				throw runtime_error("Error :: could not parse .amc file.");
			} else if (f != poses.size() + 1) {
				cout << "Expected sequential frame number: " << (poses.size() + 1) << " , got : " << line << endl;
				throw runtime_error("Error :: could not parse .amc file.");
			}

			// create a pose
			//
			skeleton_pose pose;
			pose.boneTransforms.resize(skel.bones.size());

			while(file.good() && !isdigit(file.peek())) {

				// parse the next line if not empty
				line = nextLineTrimmed(file);
				if (!line.empty()) {
					lineStream = istringstream(line);
					string boneName;
					lineStream >> boneName;

					// find bone
					int boneIndex = skel.findBone(boneName);

					if (boneIndex < 0) {
						cerr << "Expected a valid bone name, found '" << boneName << "'" << endl;
						throw runtime_error("Error :: could not parse .asf file.");
					}

					const skeleton_bone &bone = skel.bones[boneIndex];
					skeleton_bone_pose &bpose = pose.boneTransforms[boneIndex];

					if (bone.freedom & dof_root) {
						lineStream >> bpose.translation.x >> bpose.translation.y >> bpose.translation.z;
						bpose.translation *= (0.0254 / 0.45);  // scale by (0.0254/0.45) to get meters
					}
					if (bone.freedom & dof_rx) {
						lineStream >> bpose.rotation.x;
					}
					if (bone.freedom & dof_ry) {
						lineStream >> bpose.rotation.y;
					}
					if (bone.freedom & dof_rz) {
						lineStream >> bpose.rotation.z;
					}
					bpose.rotation = radians(bpose.rotation); // convert to radians

					if (lineStream.fail()) {
						cerr << "Expected frame data for " << boneName << ",  got : " << line << endl;
						throw runtime_error("Error :: could not parse .amc file.");
					}
				}
			}

			poses.push_back(pose);

		} else {
			cerr << "Expected frame number, got : " << line << endl; 
			throw runtime_error("Error :: could not parse .amc file.");
		}
	}

	cout << "Completed reading skeleton file" << endl;
}