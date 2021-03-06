/**
 * \file FEM/Output.cpp
 * 05/04/2011 LB Refactoring: Moved from rf_out_new.h
 *
 * Implementation of Output class
 * \copyright
 * Copyright (c) 2015, OpenGeoSys Community (http://www.opengeosys.org)
 *            Distributed under a Modified BSD License.
 *              See accompanying file LICENSE.txt or
 *              http://www.opengeosys.org/project/license
 */

#include "Output.h"

#include <fstream>
#include <iostream>
#include <string>

#include "Configure.h"
#include "display.h"
#include "FileToolsRF.h"
#include "makros.h"
#include "StringTools.h"

#include "mathlib.h"
#include "MathTools.h"

#include "GeoIO.h"
#include "GEOObjects.h"

#include "msh_lib.h"

#include "ElementValue.h"
#include "fem_ele.h"
#include "fem_ele_std.h"
#include "files0.h"
#include "problem.h"
#include "rf_msp_new.h"
#include "rf_pcs.h"
#include "rf_random_walk.h"
#ifdef GEM_REACT
#include "rf_REACT_GEM.h"
#endif
#include "rf_tim_new.h"
#include "vtk.h"


extern size_t max_dim;

using MeshLib::CFEMesh;
using MeshLib::CElem;
using MeshLib::CEdge;
using MeshLib::CNode;

using namespace std;

COutput::COutput()
    : GeoInfo(GEOLIB::GEODOMAIN),
      ProcessInfo(),
      tim_type_name("TIMES"),
      _id(0),
      out_amplifier(0.0),
      m_msh(NULL),
      nSteps(-1),
      _new_file_opened(false),
      dat_type_name("TECPLOT")
{
	m_pcs = NULL;
	vtk = NULL;                  // NW
	tecplot_zone_share = false;  // 10.2012. WW
	VARIABLESHARING = false;     // BG
	_time = 0;
#if defined(USE_PETSC) || \
    defined(USE_MPI)  //|| defined(other parallel libs)//03.3012. WW
	mrank = 0;
	msize = 0;
#endif
}

COutput::COutput(size_t id)
    : GeoInfo(GEOLIB::GEODOMAIN),
      ProcessInfo(),
      tim_type_name("TIMES"),
      _id(id),
      out_amplifier(0.0),
      m_msh(NULL),
      nSteps(-1),
      _new_file_opened(false),
      dat_type_name("TECPLOT")
{
	m_pcs = NULL;
	vtk = NULL;                  // NW
	tecplot_zone_share = false;  // 10.2012. WW
	VARIABLESHARING = false;     // BG
	_time = 0;
#if defined(USE_PETSC) || \
    defined(USE_MPI)  //|| defined(other parallel libs)//03.3012. WW
	mrank = 0;
	msize = 0;
#endif
}
#if defined(USE_PETSC) || \
    defined(USE_MPI)  //|| defined(other parallel libs)//03.3012. WW
void COutput::setMPI_Info(const int rank, const int size, std::string rank_str)
{
	mrank = rank;
	msize = size;
	mrank_str = rank_str;
}
#endif

/*!
   Create the instance of class CVTK
   04.2012. WW
 */
void COutput::CreateVTKInstance(void)
{
	vtk = new CVTK();
}
void COutput::init()
{
	if (getProcessType() == FiniteElement::INVALID_PROCESS)
	{
		//		ScreenMessage("COutput::init(): could not initialize process
		// pointer (process type INVALID_PROCESS) and appropriate mesh\n");
		//		ScreenMessage("COutput::init(): trying to fetch process pointer
		// using msh_type_name ... \n");
		if (!msh_type_name.empty())
		{
			_pcs = PCSGet(msh_type_name);
			if (!_pcs)
			{
				ScreenMessage(
				    "COutput::init(): failed to fetch process pointer using "
				    "msh_type_name\n");
				exit(1);
			}
		}
		else
		{
#ifndef NDEBUG
			ScreenMessage(
			    "COutput::init(): failed to fetch process pointer using "
			    "msh_type_name\n");
#endif
		}
	}

	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));

	setInternalVarialbeNames(m_msh);  // NW
}

COutput::~COutput()
{
	mmp_value_vector.clear();  // OK

	if (this->vtk != NULL) delete vtk;  // NW
}

const std::string& COutput::getGeoName() const
{
	return geo_name;
}

/**************************************************************************
   FEMLib-Method:
   Task: OUT read function
   Programing:
   06/2004 OK Implementation
   07/2004 WW Remove old files
   11/2004 OK string streaming by SB for lines
   03/2005 OK PCS_TYPE
   12/2005 OK DIS_TYPE
   12/2005 OK MSH_TYPE
   08/2008 OK MAT
   06/2010 TF formated, restructured, signature changed, use new GEOLIB data
structures
   09/2010 TF signature changed, removed some variables
**************************************************************************/
ios::pos_type COutput::Read(std::ifstream& in_str,
                            const GEOLIB::GEOObjects& geo_obj,
                            const std::string& unique_geo_name)
{
	std::string line_string;
	bool new_keyword = false;
	ios::pos_type position;
	bool new_subkeyword = false;
	std::string tec_file_name;
	ios::pos_type position_line;
	bool ok = true;
	std::stringstream in;
	string name;
	ios::pos_type position_subkeyword;

	// Schleife ueber alle Phasen bzw. Komponenten
	while (!new_keyword)
	{
		position = in_str.tellg();
		if (new_subkeyword) in_str.seekg(position_subkeyword, ios::beg);
		new_subkeyword = false;
		// SB new input		in_str.getline(buffer,MAX_ZEILE);
		// SB new input         line_string = buffer;
		line_string.clear();
		line_string = GetLineFromFile1(&in_str);
		if (line_string.size() < 1) break;

		if (Keyword(line_string)) return position;

		// subkeyword found
		if (line_string.find("$NOD_VALUES") != string::npos)
		{
			while ((!new_keyword) && (!new_subkeyword))
			{
				position_subkeyword = in_str.tellg();
				// SB input with comments  in_str >> line_string>>ws;
				line_string = GetLineFromFile1(&in_str);
				if (line_string.find("#") != string::npos) return position;
				if (line_string.find("$") != string::npos)
				{
					new_subkeyword = true;
					break;
				}
				if (line_string.size() == 0) break;  // SB: empty line
				in.str(line_string);
				in >> name;
				//_alias_nod_value_vector.push_back(name);
				_nod_value_vector.push_back(name);
				in.clear();
			}

			continue;
		}
		//--------------------------------------------------------------------
		// subkeyword found //MX
		if (line_string.find("$PCON_VALUES") != string::npos)
		{
			while ((!new_keyword) && (!new_subkeyword))
			{
				position_subkeyword = in_str.tellg();
				in_str >> line_string >> ws;
				if (line_string.find("#") != string::npos) return position;
				if (line_string.find("$") != string::npos)
				{
					new_subkeyword = true;
					break;
				}
				if (line_string.size() == 0) break;
				_pcon_value_vector.push_back(line_string);
			}
			continue;
		}

		//--------------------------------------------------------------------
		// subkeyword found
		if (line_string.find("$ELE_VALUES") != string::npos)
		{
			ok = true;
			while (ok)
			{
				position_line = in_str.tellg();
				in_str >> line_string;
				if (SubKeyword(line_string))
				{
					in_str.seekg(position_line, ios::beg);
					ok = false;
					continue;
				}
				if (Keyword(line_string)) return position;
				_ele_value_vector.push_back(line_string);
				in_str.ignore(MAX_ZEILE, '\n');
			}
			/*
			   // Commented by WW
			   // Remove files
			   tec_file_name = file_base_name + "_domain_ele" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			 */
			continue;
		}
		//--------------------------------------------------------------------
		//// Added 03.2010 JTARON
		// subkeyword found
		if (line_string.find("$RWPT_VALUES") != string::npos)
		{
			while ((!new_keyword) && (!new_subkeyword))
			{
				position_subkeyword = in_str.tellg();
				line_string = GetLineFromFile1(&in_str);
				if (line_string.find("#") != string::npos) return position;
				if (line_string.find("$") != string::npos)
				{
					new_subkeyword = true;
					break;
				}
				if (line_string.size() == 0) break;  // SB: empty line
				in.str(line_string);
				in >> name;
				_rwpt_value_vector.push_back(name);
				in.clear();
			}
			continue;
		}

		// subkeyword found
		if (line_string.find("$GEO_TYPE") != string::npos)
		{
			FileIO::GeoIO::readGeoInfo(this, in_str, geo_name, geo_obj,
			                           unique_geo_name);
			continue;
		}

		// subkeyword found
		if (line_string.find("$TIM_TYPE") != string::npos)
		{
			while ((!new_keyword) && (!new_subkeyword))
			{
				position_subkeyword = in_str.tellg();
				in_str >> line_string;
				if (line_string.size() == 0)  // SB
					break;
				if (line_string.find("#") != string::npos)
				{
					new_keyword = true;
					break;
				}
				if (line_string.find("$") != string::npos)
				{
					new_subkeyword = true;
					break;
				}
				if (line_string.find("STEPS") != string::npos)
				{
					in_str >> nSteps;
					tim_type_name = "STEPS";  // OK
					break;  // kg44 I guess that was missing..otherwise it
					        // pushes back a time_vector!
				}
				// JT 2010, reconfigured (and added RWPT)... didn't work
				if (line_string.find("STEPPING") != string::npos)
				{
					double stepping_length, stepping_end, stepping_current;
					in_str >> stepping_length >> stepping_end;
					stepping_current = stepping_length;
					while (stepping_current <= stepping_end)
					{
						time_vector.push_back(stepping_current);
						//						rwpt_time_vector.push_back(stepping_current);
						stepping_current += stepping_length;
					}
				}
				else
					time_vector.push_back(strtod(line_string.data(), NULL));
				//					rwpt_time_vector.push_back(strtod(line_string.data(),
				// NULL));
				in_str.ignore(MAX_ZEILE, '\n');
			}
			continue;
		}

		// subkeyword found
		if (line_string.find("$DAT_TYPE") != string::npos)
		{
			in_str >> dat_type_name;
			in_str.ignore(MAX_ZEILE, '\n');
			continue;
		}

		// Coordinates of each node as well as connection list is stored only
		// for the first time step; BG: 05/2011
		if (line_string.find("$VARIABLESHARING") != string::npos)
		{
			this->VARIABLESHARING = true;
			continue;
		}

		// subkeyword found
		if (line_string.find("$AMPLIFIER") != string::npos)
		{
			in_str >> out_amplifier;
			in_str.ignore(MAX_ZEILE, '\n');
			continue;
		}

		// subkeyword found
		if (line_string.find("$PCS_TYPE") != string::npos)
		{
			std::string tmp_pcs_type_name;
			in_str >> tmp_pcs_type_name;
			setProcessType(
			    FiniteElement::convertProcessType(tmp_pcs_type_name));
			in_str.ignore(MAX_ZEILE, '\n');
			/* // Comment by WW
			   // Remove files
			   tec_file_name = pcs_type_name + "_" + "domain" + "_line" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			   tec_file_name = pcs_type_name + "_" + "domain" + "_tri" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			   tec_file_name = pcs_type_name + "_" + "domain" + "_quad" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			   tec_file_name = pcs_type_name + "_" + "domain" + "_tet" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			   tec_file_name = pcs_type_name + "_" + "domain" + "_pris" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			   tec_file_name = pcs_type_name + "_" + "domain" + "_hex" +
			   TEC_FILE_EXTENSION;
			   remove(tec_file_name.c_str());
			 */
			continue;
		}

		// subkeyword found
		if (line_string.find("$DIS_TYPE") != string::npos)
		{
			std::string dis_type_name;
			in_str >> dis_type_name;
			setProcessDistributionType(
			    FiniteElement::convertDisType(dis_type_name));
			in_str.ignore(MAX_ZEILE, '\n');
			continue;
		}

		// subkeyword found
		if (line_string.find("$MSH_TYPE") != string::npos)
		{
			in_str >> msh_type_name;
			in_str.ignore(MAX_ZEILE, '\n');
			continue;
		}

		// OK
		if (line_string.find("$MMP_VALUES") != string::npos)
		{
			ok = true;
			while (ok)
			{
				position_line = in_str.tellg();
				in_str >> line_string;
				if (SubKeyword(line_string))
				{
					in_str.seekg(position_line, ios::beg);
					ok = false;
					continue;
				}
				if (Keyword(line_string)) return position;
				mmp_value_vector.push_back(line_string);
				in_str.ignore(MAX_ZEILE, '\n');
			}
			continue;
		}

		// OK
		if (line_string.find("$MFP_VALUES") != string::npos)
		{
			ok = true;
			while (ok)
			{
				position_line = in_str.tellg();
				in_str >> line_string;
				if (SubKeyword(line_string))
				{
					in_str.seekg(position_line, ios::beg);
					ok = false;
					continue;
				}
				if (Keyword(line_string)) return position;
				mfp_value_vector.push_back(line_string);
				in_str.ignore(MAX_ZEILE, '\n');
			}

			continue;
		}
		// For teplot zone share. 10.2012. WW
		if (line_string.find("$TECPLOT_ZONE_SHARE") != string::npos)
		{
			tecplot_zone_share = true;
			continue;
		}
	}
	return position;
}

/**************************************************************************
   FEMLib-Method:
   Task: write function
   Programing:
   06/2004 OK Implementation
   01/2005 OK Extensions
   12/2005 OK DIS_TYPE
   12/2005 OK MSH_TYPE
   12/2008 NW DAT_TYPE
   05/2009 OK bug fix STEPS
**************************************************************************/
void COutput::Write(fstream* out_file)
{
	//--------------------------------------------------------------------
	// KEYWORD
	*out_file << "#OUTPUT"
	          << "\n";
	//--------------------------------------------------------------------
	// PCS_TYPE
	*out_file << " $PCS_TYPE"
	          << "\n"
	          << "  ";
	*out_file << convertProcessTypeToString(getProcessType()) << "\n";
	//--------------------------------------------------------------------
	// NOD_VALUES
	*out_file << " $NOD_VALUES"
	          << "\n";
	size_t nod_value_vector_size(_nod_value_vector.size());
	for (size_t i = 0; i < nod_value_vector_size; i++)
		*out_file << "  " << _nod_value_vector[i] << "\n";
	//--------------------------------------------------------------------
	// ELE_VALUES
	*out_file << " $ELE_VALUES"
	          << "\n";
	size_t ele_value_vector_size(_ele_value_vector.size());
	for (size_t i = 0; i < ele_value_vector_size; i++)
		*out_file << "  " << _ele_value_vector[i] << "\n";
	//--------------------------------------------------------------------
	// GEO_TYPE
	*out_file << " $GEO_TYPE"
	          << "\n";
	*out_file << "  ";
	*out_file << getGeoTypeAsString() << " " << geo_name << "\n";
	//--------------------------------------------------------------------
	// TIM_TYPE
	*out_file << " $TIM_TYPE"
	          << "\n";
	if (tim_type_name == "STEPS")
		*out_file << "  " << tim_type_name << " " << nSteps << "\n";
	else
	{
		size_t time_vector_size(time_vector.size());
		for (size_t i = 0; i < time_vector_size; i++)
			*out_file << "  " << time_vector[i] << "\n";
	}

	// DIS_TYPE
	//	if (_dis_type_name.size() > 0) {
	//		*out_file << " $DIS_TYPE" << "\n";
	//		*out_file << "  ";
	//		*out_file << _dis_type_name << "\n";
	//	}
	if (getProcessDistributionType() != FiniteElement::INVALID_DIS_TYPE)
	{
		*out_file << " $DIS_TYPE"
		          << "\n";
		*out_file << "  ";
		*out_file << convertDisTypeToString(getProcessDistributionType())
		          << "\n";
	}

	// MSH_TYPE
	if (msh_type_name.size() > 0)
	{
		*out_file << " $MSH_TYPE"
		          << "\n";
		*out_file << "  ";
		*out_file << msh_type_name << "\n";
	}
	//--------------------------------------------------------------------
	// DAT_TYPE
	*out_file << " $DAT_TYPE"
	          << "\n";
	*out_file << "  ";
	*out_file << dat_type_name << "\n";
	//--------------------------------------------------------------------
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   08/2004 OK Implementation
   03/2005 OK MultiMSH
   08/2005 WW Changes for MultiMSH
   12/2005 OK VAR,MSH,PCS concept
   07/2007 NW Multi Mesh Type
**************************************************************************/
void COutput::NODWriteDOMDataTEC()
{
	int te = 0;
	string eleType;
	string tec_file_name;
	//----------------------------------------------------------------------
	// Tests
	// OK4704
	if ((_nod_value_vector.size() == 0) && (mfp_value_vector.size() == 0))
		return;
	//......................................................................
	// MSH
	// m_msh = MeshLib::FEMGet(pcs_type_name);
	//  m_msh = GetMSH();
	if (!m_msh)
	{
		cout << "Warning in COutput::NODWriteDOMDataTEC() - no MSH data"
		     << "\n";
		return;
	}
	//======================================================================
	vector<int> mesh_type_list;  // NW
	if (m_msh->getNumberOfLines() > 0) mesh_type_list.push_back(1);
	if (m_msh->getNumberOfQuads() > 0) mesh_type_list.push_back(2);
	if (m_msh->getNumberOfHexs() > 0) mesh_type_list.push_back(3);
	if (m_msh->getNumberOfTris() > 0) mesh_type_list.push_back(4);
	if (m_msh->getNumberOfTets() > 0) mesh_type_list.push_back(5);
	if (m_msh->getNumberOfPrisms() > 0) mesh_type_list.push_back(6);
	if (m_msh->getNumberOfPyramids() > 0) mesh_type_list.push_back(7);

	// Output files for each mesh type
	// NW
	for (int i = 0; i < (int)mesh_type_list.size(); i++)
	{
		te = mesh_type_list[i];
		//----------------------------------------------------------------------
		// File name handling
		tec_file_name = file_base_name + "_" + "domain";
		if (msh_type_name.size() > 0)  // MultiMSH
			tec_file_name += "_" + msh_type_name;
		if (getProcessType() != FiniteElement::INVALID_PROCESS)  // PCS
			tec_file_name += "_" + convertProcessTypeToString(getProcessType());
		//======================================================================
		switch (te)  // NW
		{
			case 1:
				tec_file_name += "_line";
				eleType = "QUADRILATERAL";
				break;
			case 2:
				tec_file_name += "_quad";
				eleType = "QUADRILATERAL";
				break;
			case 3:
				tec_file_name += "_hex";
				eleType = "BRICK";
				break;
			case 4:
				tec_file_name += "_tri";
				eleType = "QUADRILATERAL";
				break;
			case 5:
				tec_file_name += "_tet";
				eleType = "TETRAHEDRON";
				break;
			case 6:
				tec_file_name += "_pris";
				eleType = "BRICK";
				break;
			case 7:
				tec_file_name += "_pyra";
				eleType = "BRICK";
				break;
		}
/*
   if(m_msh->msh_no_line>0)
   {
      tec_file_name += "_line";
      eleType = "QUADRILATERAL";
     te=1;
   }
   else if (m_msh->msh_no_quad>0)
   {
      tec_file_name += "_quad";
      eleType = "QUADRILATERAL";
   te=2;
   }
   else if (m_msh->msh_no_hexs>0)
   {
   tec_file_name += "_hex";
   eleType = "BRICK";
   te=3;
   }
   else if (m_msh->msh_no_tris>0)
   {
   tec_file_name += "_tri";
   //???Who was this eleType = "TRIANGLE";
   eleType = "QUADRILATERAL";
   te=4;
   }
   else if (m_msh->msh_no_tets>0)
   {
   tec_file_name += "_tet";
   eleType = "TETRAHEDRON";
   te=5;
   }
   else if (m_msh->msh_no_pris>0)
   {
   tec_file_name += "_pris";
   eleType = "BRICK";
   te=6;
   }
 */
#if defined(USE_PETSC)  //|| defined(other parallel libs)//03.3012. WW
		tec_file_name += "_" + mrank_str;
		std::cout << "Tecplot filename: " << tec_file_name << "\n";
#endif
		tec_file_name += TEC_FILE_EXTENSION;
		// WW
		if (!_new_file_opened) remove(tec_file_name.c_str());
		fstream tec_file(tec_file_name.data(), ios::app | ios::out);
		tec_file.setf(ios::scientific, ios::floatfield);
		tec_file.precision(12);
		if (!tec_file.good()) return;
#ifdef SUPERCOMPUTER
		// kg44 buffer the output
		char mybuf1[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
		tec_file.rdbuf()->pubsetbuf(mybuf1, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
#endif
		//
		WriteTECHeader(tec_file, te, eleType);
		WriteTECNodeData(tec_file);

		// 08.2012. WW
		if (tecplot_zone_share)
		{
			if (!_new_file_opened) WriteTECElementData(tec_file, te);
		}
		else
		{
			WriteTECElementData(tec_file, te);
		}

		tec_file.close();  // kg44 close file
	}
}

/**************************************************************************
   FEMLib-Method:
   Programing:
   08/2004 OK Implementation
   08/2004 WW Output node variables by their names given in .out file
   03/2005 OK MultiMSH
   08/2005 WW Correction of node index
   12/2005 OK Mass transport specifics
   OK ??? too many specifics
**************************************************************************/
void COutput::WriteTECNodeData(fstream& tec_file)
{
	const size_t nName(_nod_value_vector.size());
	double val_n = 0.;  // WW
	int nidx, nidx_dm[3];
	vector<int> NodeIndex(nName);
	string nod_value_name;  // OK
	CNode* node = NULL;
	CRFProcess* deform_pcs = NULL;  // 23.01.2012. WW. nulltpr

	int timelevel;
	//	m_msh = GetMSH();
	CRFProcess* m_pcs_out = NULL;
	// MSH
	for (size_t k = 0; k < nName; k++)
	{
		m_pcs = PCSGet(_nod_value_vector[k], true);
		if (m_pcs != NULL)
		{
			NodeIndex[k] = m_pcs->GetNodeValueIndex(_nod_value_vector[k],
			                                        true);  // JT Latest.
			if ((m_pcs->getProcessType() == FiniteElement::DEFORMATION) ||
			    (m_pcs->getProcessType() == FiniteElement::DEFORMATION_FLOW) ||
			    (m_pcs->getProcessType() == FiniteElement::DEFORMATION_H2))
			{
				deform_pcs = m_pcs;
			}
		}
	}

	if (deform_pcs)  // 23.01.2012. WW.
	{
		nidx_dm[0] = deform_pcs->GetNodeValueIndex("DISPLACEMENT_X1") + 1;
		nidx_dm[1] = deform_pcs->GetNodeValueIndex("DISPLACEMENT_Y1") + 1;
		if (max_dim > 1)
			nidx_dm[2] = deform_pcs->GetNodeValueIndex("DISPLACEMENT_Z1") + 1;
		else
			nidx_dm[2] = -1;
	}
	// 08.2012. WW
	bool out_coord = true;
	if (tecplot_zone_share && _new_file_opened) out_coord = false;
	for (size_t j = 0; j < m_msh->GetNodesNumber(false); j++)
	{
		node = m_msh->nod_vector[j];  // 23.01.2013. WW
		const size_t n_id = node->GetIndex();

		if (out_coord)  // 08.2012. WW
		{
			// XYZ
			const double* x = node->getData();  // 23.01.2013. WW

			// Amplifying DISPLACEMENTs
			if (deform_pcs)  // 23.01.2012. WW.
			{
				for (size_t i = 0; i < max_dim + 1; i++)
					tec_file << x[i] +
					                out_amplifier *
					                    m_pcs->GetNodeValue(n_id, nidx_dm[i])
					         << " ";
				for (size_t i = max_dim + 1; i < 3; i++)
					tec_file << x[i] << " ";
			}
			else
			{
				for (size_t i = 0; i < 3; i++)
					tec_file << x[i] << " ";
			}
		}
		// NOD values
		// Mass transport
		//     if(pcs_type_name.compare("MASS_TRANSPORT")==0){
		if (getProcessType() == FiniteElement::MASS_TRANSPORT)
			for (size_t i = 0; i < _nod_value_vector.size(); i++)
			{
				std::string nod_value_name = _nod_value_vector[i];
				for (size_t l = 0; l < pcs_vector.size(); l++)
				{
					m_pcs = pcs_vector[l];
					//					if
					//(m_pcs->pcs_type_name.compare("MASS_TRANSPORT") == 0) {
					if (m_pcs->getProcessType() ==
					    FiniteElement::MASS_TRANSPORT)
					{
						timelevel = 0;
						for (size_t m = 0;
						     m < m_pcs->nod_val_name_vector.size();
						     m++)
							if (m_pcs->nod_val_name_vector[m].compare(
							        nod_value_name) == 0)
							{
								m_pcs_out =
								    PCSGet(FiniteElement::MASS_TRANSPORT,
								           nod_value_name);
								if (!m_pcs_out) continue;
								if (timelevel == 1)
								{
									nidx = m_pcs_out->GetNodeValueIndex(
									           nod_value_name) +
									       timelevel;
									tec_file << m_pcs_out->GetNodeValue(
									                n_id, nidx) << " ";
								}
								timelevel++;
							}
					}
				}
			}
		else
		{
			for (size_t k = 0; k < nName; k++)
			{
				m_pcs = GetPCS(_nod_value_vector[k]);
				if (m_pcs != NULL)
				{  // WW

					if (NodeIndex[k] > -1)
					{
						if ((m_pcs->type == 14) &&
						    (_nod_value_vector[k] == "HEAD"))
						{   // HEAD output for RICHARDS_FLOW (unconfined GW)
							// 5.3.07 JOD
							double rhow;
							double dens_arg[3];
							CRFProcess* m_pcs_transport = NULL;
							m_pcs_transport = PCSGet("MASS_TRANSPORT");
							if (m_pcs_transport)
							{
								dens_arg[2] = m_pcs_transport->GetNodeValue(
								    n_id, 1);  // first component!!!
								rhow = mfp_vector[0]->Density(
								    dens_arg);  // first phase!!!  dens_arg
							}
							else
								rhow = mfp_vector[0]->Density();
							val_n =
							    m_pcs->GetNodeValue(
							        n_id,
							        m_pcs->GetNodeValueIndex("PRESSURE1") + 1) /
							        (rhow * 9.81) +
							    m_msh->nod_vector[m_msh->nod_vector[j]
							                          ->GetIndex()]
							        ->getData()[2];
							tec_file << val_n << " ";
						}
						else
						{
							val_n =
							    m_pcs->GetNodeValue(n_id, NodeIndex[k]);  // WW
							tec_file << val_n << " ";
							if ((m_pcs->type == 1212 || m_pcs->type == 42) &&
							    _nod_value_vector[k].find("SATURATION") !=
							        string::npos)  // WW
								tec_file << 1. - val_n << " ";
						}
					}
				}
			}
			// OK4704
			for (size_t k = 0; k < mfp_value_vector.size(); k++)
				// tec_file <<
				// MFPGetNodeValue(m_msh->nod_vector[j]->GetIndex(),mfp_value_vector[k])
				// << " "; //NB
				tec_file
				    << MFPGetNodeValue(
				           n_id, mfp_value_vector[k],
				           atoi(
				               &mfp_value_vector[k][mfp_value_vector[k].size() -
				                                    1]) -
				               1) << " ";  // NB: MFP output for all phases
		}
		tec_file << "\n";
	}
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   08/2004 OK Implementation
   03/2005 OK MultiMSH
   08/2005 WW Wite for MultiMSH
   12/2005 OK GetMSH
   07/2007 NW Multi Mesh Type
**************************************************************************/
void COutput::WriteTECElementData(fstream& tec_file, int e_type)
{
	for (size_t i = 0; i < m_msh->ele_vector.size(); i++)
	{
		if (!m_msh->ele_vector[i]->GetMark()) continue;
		// NW
		if (m_msh->ele_vector[i]->GetElementType() == e_type)
			m_msh->ele_vector[i]->WriteIndex_TEC(tec_file);
	}
}

/**************************************************************************
   FEMLib-Method:
   Programing:
   08/2004 OK Implementation
   08/2004 WW Header by the names gives in .out file
   03/2005 OK MultiMSH
   04/2005 WW Output active elements only
   08/2005 WW Output by MSH
   12/2005 OK GetMSH
**************************************************************************/
void COutput::WriteTECHeader(fstream& tec_file, int e_type, string e_type_name)
{
	// MSH
	//	m_msh = GetMSH();

	// OK411
	size_t no_elements = 0;
	const size_t mesh_ele_vector_size(m_msh->ele_vector.size());
	for (size_t i = 0; i < mesh_ele_vector_size; i++)
		if (m_msh->ele_vector[i]->GetMark())
			if (m_msh->ele_vector[i]->GetElementType() == e_type) no_elements++;
	//--------------------------------------------------------------------
	// Write Header I: variables
	CRFProcess* pcs = NULL;  // WW
	const size_t nName(_nod_value_vector.size());
	tec_file << "VARIABLES  = \"X\",\"Y\",\"Z\"";
	for (size_t k = 0; k < nName; k++)
	{
		tec_file << ", \"" << _nod_value_vector[k] << "\"";
		//-------------------------------------WW
		pcs = GetPCS(_nod_value_vector[k]);
		if (pcs != NULL)
			if ((pcs->type == 1212 || pcs->type == 42) &&
			    _nod_value_vector[k].find("SATURATION") != string::npos)
				tec_file << ", SATURATION2";
		//-------------------------------------WW
	}
	const size_t mfp_value_vector_size(mfp_value_vector.size());
	for (size_t k = 0; k < mfp_value_vector_size; k++)
		// NB
		tec_file << ", \"" << mfp_value_vector[k] << "\"";

	// PCON
	// MX
	const size_t nPconName(_pcon_value_vector.size());
	for (size_t k = 0; k < nPconName; k++)
		// MX
		tec_file << ", " << _pcon_value_vector[k] << "";
	tec_file << "\n";

	//--------------------------------------------------------------------
	// Write Header II: zone
	tec_file << "ZONE T=\"";
	tec_file << _time << "s\", ";
	// OK411
	tec_file << "N=" << m_msh->GetNodesNumber(false) << ", ";
	tec_file << "E=" << no_elements << ", ";
	tec_file << "F="
	         << "FEPOINT"
	         << ", ";
	tec_file << "ET=" << e_type_name;
	tec_file << "\n";
	//--------------------------------------------------------------------
	// Write Header III: solution time			; BG 05/2011
	tec_file << "STRANDID=1, SOLUTIONTIME=";
	tec_file << _time;  // << "s\"";
	tec_file << "\n";

	//--------------------------------------------------------------------
	// Write Header IV: Variable sharing		; BG 05/2011
	if (this->VARIABLESHARING == true)
	{
		// int timestep = this->getNSteps;
		// if (this->
	}
	//
	if (_new_file_opened && tecplot_zone_share)  // 08.2012. WW
	{
		tec_file << "VARSHARELIST=([1-3]=1)"
		         << "\n";
		tec_file << "CONNECTIVITYSHAREZONE=1"
		         << "\n";
	}
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   09/2004 OK Implementation
   01/2006 OK VAR,PCS,MSH concept
**************************************************************************/
void COutput::ELEWriteDOMDataTEC()
{
	//----------------------------------------------------------------------
	if (_ele_value_vector.empty()) return;
	//----------------------------------------------------------------------
	// File handling
	//......................................................................
	string tec_file_name = file_base_name + "_domain" + "_ele";
	if (getProcessType() != FiniteElement::INVALID_PROCESS)  // PCS
		// 09/2010 TF msh_type_name;
		tec_file_name += "_" + convertProcessTypeToString(getProcessType());
	if (msh_type_name.size() > 1)  // MSH
		tec_file_name += "_" + msh_type_name;
	tec_file_name += TEC_FILE_EXTENSION;
	// WW
	if (!_new_file_opened) remove(tec_file_name.c_str());
	//......................................................................
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif

	//--------------------------------------------------------------------
	WriteELEValuesTECHeader(tec_file);
	WriteELEValuesTECData(tec_file);
	//--------------------------------------------------------------------
	tec_file.close();  // kg44 close file
}

void COutput::WriteELEValuesTECHeader(fstream& tec_file)
{
	// Write Header I: variables
	tec_file << "VARIABLES = \"X\",\"Y\",\"Z\",\"VX\",\"VY\",\"VZ\"";
	for (size_t i = 0; i < _ele_value_vector.size(); i++)
		// WW
		if (_ele_value_vector[i].find("VELOCITY") == string::npos)
			tec_file << "," << _ele_value_vector[i];
	tec_file << "\n";

	// Write Header II: zone
	tec_file << "ZONE T=\"";
	tec_file << _time << "s\", ";
	tec_file << "I=" << (long)m_msh->ele_vector.size() << ", ";
	tec_file << "F=POINT"
	         << ", ";
	tec_file << "C=BLACK";
	tec_file << "\n";
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   09/2004 OK Implementation
   11/2005 OK MSH
   01/2006 OK
**************************************************************************/
void COutput::WriteELEValuesTECData(fstream& tec_file)
{
	// CRFProcess* m_pcs = NULL;
	CRFProcess* m_pcs_2 = NULL;
	if (_ele_value_vector.empty()) return;
	//		m_pcs = GetPCS_ELE(_ele_value_vector[0]);

	vector<bool> skip;  // CB
	size_t no_ele_values = _ele_value_vector.size();
	bool out_element_vel = false;
	for (size_t j = 0; j < no_ele_values; j++)  // WW
	{
		if (_ele_value_vector[j].find("VELOCITY") != string::npos)
		{
			out_element_vel = true;
			// break;  // CB: allow output of velocity AND other ele values
			skip.push_back(false);
		}
		else
		{
			m_pcs_2 = GetPCS_ELE(_ele_value_vector[j]);
			skip.push_back(true);
		}
	}
	vector<int> ele_value_index_vector(no_ele_values);
	GetELEValuesIndexVector(ele_value_index_vector);

	MeshLib::CElem* m_ele = NULL;
	FiniteElement::ElementValue* gp_ele = NULL;
	for (size_t i = 0; i < m_msh->ele_vector.size(); i++)
	{
		m_ele = m_msh->ele_vector[i];
		double const* xyz(m_ele->GetGravityCenter());
		tec_file << xyz[0] << " " << xyz[1] << " " << xyz[2] << " ";
		if (out_element_vel)  // WW
		{
			if (PCSGet(FiniteElement::FLUID_MOMENTUM))  // PCH 16.11 2009
			{
				CRFProcess* pch_pcs = PCSGet(FiniteElement::FLUID_MOMENTUM);

				tec_file << pch_pcs->GetElementValue(
				                i,
				                pch_pcs->GetElementValueIndex("VELOCITY1_X") +
				                    1) << " ";
				tec_file << pch_pcs->GetElementValue(
				                i,
				                pch_pcs->GetElementValueIndex("VELOCITY1_Y") +
				                    1) << " ";
				tec_file << pch_pcs->GetElementValue(
				                i,
				                pch_pcs->GetElementValueIndex("VELOCITY1_Z") +
				                    1) << " ";
			}
			else
			{
				gp_ele = ele_gp_value[i];
				tec_file << gp_ele->Velocity(0, 0) << " ";
				tec_file << gp_ele->Velocity(1, 0) << " ";
				tec_file << gp_ele->Velocity(2, 0) << " ";
			}
		}
		for (size_t j = 0; j < ele_value_index_vector.size(); j++)
		{
			if (skip[j])  // CB: allow output of velocity AND other ele values
			{
				tec_file << m_pcs_2->GetElementValue(
				                i, ele_value_index_vector[j]) << " ";
			}
		}
		/*
		   int j;
		   int eidx;
		   char ele_value_char[20];
		   int no_ele_values = (int)ele_value_vector.size();
		   for(j=0;j<no_ele_values;j++){
		   sprintf(ele_value_char,"%s",ele_value_vector[j].data());
		   eidx = PCSGetELEValueIndex(ele_value_char);
		   tec_file << ElGetElementVal(i,eidx) << " ";
		   }
		 */
		tec_file << "\n";
	}

	ele_value_index_vector.clear();
	skip.clear();
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   08/2004 OK Implementation
   08/2004 WW Output by the order of distance to one end of the polyline
        OK this should be done in the PLY method
   08/2005 WW Changes due to the Geometry element class applied
           Remove existing files
   12/2005 OK Mass transport specifics
   12/2005 OK VAR,MSH,PCS concept
   12/2005 WW Output stress invariants
   08/2006 OK FLUX
   10/2008 OK MFP values
   07/2010 TF substituted GEOGetPLYByName
**************************************************************************/
double COutput::NODWritePLYDataTEC(int number)
{
	if (_nod_value_vector.empty() && mfp_value_vector.empty())
		return 0.0;

	GEOLIB::Polyline const* const ply(
	    dynamic_cast<GEOLIB::Polyline const* const>(this->getGeoObj()));
	if (this->getGeoType() != GEOLIB::POLYLINE || ply == NULL)
	{
		std::cerr
		    << "COutput::NODWritePLYDataTEC geometric object is not a polyline"
		    << "\n";
		return 0.0;
	}

	long gnode;
	bool bdummy = false;
	int stress_i[6], strain_i[6];
	double ss[6];
	double val_n = 0.;


	// File handling
	std::string tec_file_name = file_base_name + "_ply_" + geo_name + "_t" +
	                            number2str<size_t>(_id);
	if (getProcessType() != FiniteElement::INVALID_PROCESS)
		tec_file_name += "_" + convertProcessTypeToString(getProcessType());
	if (msh_type_name.size() > 0) tec_file_name += "_" + msh_type_name;
	tec_file_name += TEC_FILE_EXTENSION;

	if (!_new_file_opened)
		remove(tec_file_name.c_str());

	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	if (!tec_file.good()) return 0.0;

	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	if (!m_msh)
		cout << "Warning in COutput::NODWritePLYDataTEC - no MSH data" << endl;
	else
		m_msh->SwitchOnQuadraticNodes(false);

	if (getProcessType() == FiniteElement::INVALID_PROCESS)
		m_pcs = NULL;
	else
		m_pcs = PCSGet(getProcessType());

	CRFProcess* dm_pcs = PCSGetDeformation();

	//--------------------------------------------------------------------
	// NIDX for output variables
	size_t no_variables(_nod_value_vector.size());
	std::vector<int> NodeIndex(no_variables);
	GetNodeIndexVector(NodeIndex);
	//--------------------------------------------------------------------
	// Write header
	if (!_new_file_opened)
	{
		// project_title;
		std::string project_title_string = "Profiles along polylines";

		if (dat_type_name.compare("GNUPLOT") != 0)  // 5.3.07 JOD
			tec_file << " TITLE = \"" << project_title_string << "\""
			         << "\n";
		else
			tec_file << "# ";

		tec_file << " VARIABLES = \"DIST\" ";
		for (size_t k = 0; k < no_variables; k++)
		{
			tec_file << "\"" << _nod_value_vector[k] << "\" ";
			//-------------------------------------WW
			m_pcs = GetPCS(_nod_value_vector[k]);
			if (m_pcs && m_pcs->type == 1212 &&
			    _nod_value_vector[k].find("SATURATION") != string::npos)
				tec_file << "SATURATION2 ";
			//-------------------------------------WW
			if (_nod_value_vector[k].compare("FLUX") == 0)
				tec_file << "FLUX_INNER"
				         << " ";
		}
		//....................................................................
		// OK4709
		for (size_t k = 0; k < mfp_value_vector.size(); k++)
			tec_file << "\"" << mfp_value_vector[k] << "\" ";
		//....................................................................
		// WW: M specific data
		if (dm_pcs)  // WW

			tec_file << " p_(1st_Invariant) "
			         << " q_(2nd_Invariant)  "
			         << " Effective_Strain";
		tec_file << "\n";
	}
	//....................................................................
	// WW: M specific data
	size_t ns = 4;
	if (dm_pcs)  // WW
	{
		stress_i[0] = dm_pcs->GetNodeValueIndex("STRESS_XX");
		stress_i[1] = dm_pcs->GetNodeValueIndex("STRESS_YY");
		stress_i[2] = dm_pcs->GetNodeValueIndex("STRESS_ZZ");
		stress_i[3] = dm_pcs->GetNodeValueIndex("STRESS_XY");
		strain_i[0] = dm_pcs->GetNodeValueIndex("STRAIN_XX");
		strain_i[1] = dm_pcs->GetNodeValueIndex("STRAIN_YY");
		strain_i[2] = dm_pcs->GetNodeValueIndex("STRAIN_ZZ");
		strain_i[3] = dm_pcs->GetNodeValueIndex("STRAIN_XY");
		if (max_dim == 2)  // 3D
		{
			ns = 6;
			stress_i[4] = dm_pcs->GetNodeValueIndex("STRESS_XZ");
			stress_i[5] = dm_pcs->GetNodeValueIndex("STRESS_YZ");
			strain_i[4] = dm_pcs->GetNodeValueIndex("STRAIN_XZ");
			strain_i[5] = dm_pcs->GetNodeValueIndex("STRAIN_YZ");
		}
	}
	//......................................................................
	// , I=" << NodeListLength << ", J=1, K=1, F=POINT" << "\n";
	if (dat_type_name.compare("GNUPLOT") == 0)  // 6/2012 JOD
		tec_file << "# ";

	tec_file << " ZONE T=\"TIME=" << _time << "\""
	         << "\n";
	//----------------------------------------------------------------------
	// Write data
	//======================================================================
	double flux_sum = 0.0;  // OK
	double flux_nod;

	m_msh->SwitchOnQuadraticNodes(false);  // WW
	// NOD at PLY
	std::vector<long> nodes_vector;

	CGLPolyline* m_ply = GEOGetPLYByName(geo_name);
	//   m_msh->GetNODOnPLY(m_ply, old_nodes_vector); // TF

	double tmp_min_edge_length(m_msh->getMinEdgeLength());
	m_msh->setMinEdgeLength(m_ply->epsilon);
	m_msh->GetNODOnPLY(ply, nodes_vector);  // TF
	std::vector<double> interpolation_points;
	m_msh->getPointsForInterpolationAlongPolyline(ply, interpolation_points);
	m_msh->setMinEdgeLength(tmp_min_edge_length);

	//   std::cout << "size of nodes_vector: " << nodes_vector.size() << ", size
	//   of old_nodes_vector: " << old_nodes_vector.size() << "\n";
	// bool b_specified_pcs = (m_pcs != NULL); //NW m_pcs =
	// PCSGet(pcs_type_name);
	for (size_t j(0); j < nodes_vector.size(); j++)
	{
		//		tec_file << m_ply->getSBuffer()[j] << " ";
		tec_file << interpolation_points[j] << " ";
		// WW
		//		long old_gnode = nodes_vector[m_ply->getOrderedPoints()[j]];
		gnode = nodes_vector[j];
		for (size_t k = 0; k < no_variables; k++)
		{
			// if(!(_nod_value_vector[k].compare("FLUX")==0))  // removed JOD,
			// does not work for multiple flow processes
			// if (!b_specified_pcs) //NW
			if (msh_type_name != "COMPARTMENT")  // JOD 4.10.01
				m_pcs = PCSGet(_nod_value_vector[k], bdummy);

			if (!m_pcs)
			{
				cout << "Warning in COutput::NODWritePLYDataTEC - no PCS data"
				     << endl;
				tec_file
				    << "Warning in COutput::NODWritePLYDataTEC - no PCS data"
				    << "\n";
				return 0.0;
			}
			// WW
			//			double old_val_n = m_pcs->GetNodeValue(old_gnode,
			// NodeIndex[k]);
			val_n = m_pcs->GetNodeValue(gnode, NodeIndex[k]);
			//			tec_file << old_val_n << " ";
			tec_file << val_n << " ";
			if (m_pcs->type == 1212 &&
			    (_nod_value_vector[k].find("SATURATION") != string::npos))
				tec_file << 1. - val_n << " ";

			if (_nod_value_vector[k].compare("FLUX") == 0)
			{
				if (aktueller_zeitschritt == 0)  // OK
					flux_nod = 0.0;
				else
					flux_nod = NODFlux(gnode);
				tec_file << flux_nod << " ";
				// flux_sum += abs(m_pcs->eqs->b[gnode]);
				flux_sum += abs(flux_nod);
				// OK cout << gnode << " " << flux_nod << " " << flux_sum <<
				// endl;
			}
		}
		if (dm_pcs)  // WW
		{
			for (size_t i = 0; i < ns; i++)
				ss[i] = dm_pcs->GetNodeValue(gnode, stress_i[i]);
			tec_file << -DeviatoricStress(ss) / 3.0 << " ";
			tec_file << sqrt(3.0 *
			                 TensorMutiplication2(
			                     ss, ss, m_msh->GetCoordinateFlag() / 10) /
			                 2.0) << "  ";
			for (size_t i = 0; i < ns; i++)
				ss[i] = dm_pcs->GetNodeValue(gnode, strain_i[i]);
			DeviatoricStress(ss);
			tec_file << sqrt(
			    3.0 *
			    TensorMutiplication2(ss, ss, m_msh->GetCoordinateFlag() / 10) /
			    2.0);
		}

		// MFP //OK4704
		// OK4704
		for (size_t k = 0; k < mfp_value_vector.size(); k++)
			//     tec_file << MFPGetNodeValue(gnode,mfp_value_vector[k],0) << "
			//     "; //NB
			tec_file
			    << MFPGetNodeValue(
			           gnode, mfp_value_vector[k],
			           atoi(&mfp_value_vector[k][mfp_value_vector[k].size() -
			                                     1]) -
			               1) << " ";  // NB: MFP output for all phases

		tec_file << "\n";
	}
	tec_file.close();  // kg44 close file
	return flux_sum;
}

/**************************************************************************
   FEMLib-Method:
   Task:
   Programing:
   08/2004 OK Implementation
   08/2005 WW MultiMesh
   12/2005 OK VAR,MSH,PCS concept
   12/2005 WW Output stress invariants
   10/2010 TF changed access to process type
**************************************************************************/
void COutput::NODWritePNTDataTEC(double time_current, int time_step_number)
{
	const long msh_node_number(
	    m_msh->GetNODOnPNT(static_cast<const GEOLIB::Point*>(getGeoObj())));
	if (msh_node_number < 0)  // 11.06.2012. WW
		return;
	ScreenMessage2("Node %d found for %s\n", msh_node_number, geo_name.data());

#ifdef USE_PETSC
	if (!m_msh->isNodeLocal(msh_node_number)) return;
#endif

	CRFProcess* dm_pcs = NULL;
	for (size_t i = 0; i < pcs_vector.size(); i++)
		//		if (pcs_vector[i]->pcs_type_name.find("DEFORMATION") !=
		// string::npos) { TF
		if (isDeformationProcess(pcs_vector[i]->getProcessType()))
		{
			dm_pcs = pcs_vector[i];
			break;
		}

	// File handling
	std::string tec_file_name(file_base_name + "_time_");
	addInfoToFileName(tec_file_name, true, true, true);

	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	//......................................................................
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);

	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	// Tests
	//......................................................................
	//	CFEMesh* m_msh = GetMSH();
	//	m_msh = GetMSH();
	if (!m_msh)
	{
		cout << "Warning in COutput::NODWritePNTDataTEC - no MSH data: "
		     << endl;
		tec_file << "Warning in COutput::NODWritePNTDataTEC - no MSH data: "
		         << "\n";
		tec_file.close();
		return;
	}

	//----------------------------------------------------------------------
	// NIDX for output variables
	size_t no_variables(_nod_value_vector.size());
	vector<int> NodeIndex(no_variables);
	GetNodeIndexVector(NodeIndex);
	//--------------------------------------------------------------------
	bool isCSV = (dat_type_name.compare("CSV") == 0);
	bool isGNUPLOT = (dat_type_name.compare("GNUPLOT") == 0);
	const std::string sep = isCSV ? ", " : " ";
	// Write header
	if (time_step_number == 0)  // WW  Old: if(time_step_number==1)
	{
		// project_title;
		if (!isCSV)
		{
			if (!isGNUPLOT)
			{  // 6/2012 JOD
				const std::string project_title_string("Time curves in points");
				tec_file << " TITLE = \"" << project_title_string << "\""
				         << "\n";
			}
			else
			{
				tec_file << "# ";
			}
			tec_file << " VARIABLES = ";
		}
		tec_file << "\"TIME\"";

		//    if(pcs_type_name.compare("RANDOM_WALK")==0)
		if (getProcessType() == FiniteElement::RANDOM_WALK)
			tec_file << "leavingParticles ";
		for (size_t k = 0; k < no_variables; k++)  // WW
		{
			tec_file << sep << "\"" << _nod_value_vector[k] << "\"";
			//-------------------------------------WW
			m_pcs = GetPCS(_nod_value_vector[k]);
			if (m_pcs && m_pcs->type == 1212 &&
			    _nod_value_vector[k].find("SATURATION") != string::npos)
				tec_file << "SATURATION2 ";
			//-------------------------------------WW
		}
		// OK411
		for (size_t k = 0; k < mfp_value_vector.size(); k++)
			// NB MFP data names for multiple phases
			tec_file << " \"" << mfp_value_vector[k] << "\"" << sep;

        if (dm_pcs && !isCSV)  // WW
			tec_file << " p_(1st_Invariant) "
			         << " q_(2nd_Invariant)  "
			         << " Effective_Strain";
		tec_file << "\n";

		if (!isCSV)
		{
			if (isGNUPLOT)         // 5.3.07 JOD
				tec_file << "# ";  // comment

			if (geo_name.find("POINT") == std::string::npos)
				tec_file << " ZONE T=\"POINT="
				         //, I=" << anz_zeitschritte << ", J=1, K=1, F=POINT" <<
				         //"\n";
				         << getGeoTypeAsString() << geo_name << "\""
				         << "\n";
			else
				tec_file << " ZONE T=\"POINT=" << geo_name << "\""
				         << "\n";  //, I=" << anz_zeitschritte << ", J=1, K=1,
			// F=POINT" << "\n";
		}
	}

	// For deformation
	size_t ns = 4;
	int stress_i[6], strain_i[6];
	double ss[6];
	if (dm_pcs)  // WW
	{
		stress_i[0] = dm_pcs->GetNodeValueIndex("STRESS_XX");
		stress_i[1] = dm_pcs->GetNodeValueIndex("STRESS_YY");
		stress_i[2] = dm_pcs->GetNodeValueIndex("STRESS_ZZ");
		stress_i[3] = dm_pcs->GetNodeValueIndex("STRESS_XY");
		strain_i[0] = dm_pcs->GetNodeValueIndex("STRAIN_XX");
		strain_i[1] = dm_pcs->GetNodeValueIndex("STRAIN_YY");
		strain_i[2] = dm_pcs->GetNodeValueIndex("STRAIN_ZZ");
		strain_i[3] = dm_pcs->GetNodeValueIndex("STRAIN_XY");
		if (m_msh->GetCoordinateFlag() / 10 == 3)  // 3D
		{
			ns = 6;
			stress_i[4] = dm_pcs->GetNodeValueIndex("STRESS_XZ");
			stress_i[5] = dm_pcs->GetNodeValueIndex("STRESS_YZ");
			strain_i[4] = dm_pcs->GetNodeValueIndex("STRAIN_XZ");
			strain_i[5] = dm_pcs->GetNodeValueIndex("STRAIN_YZ");
		}
	}
	//--------------------------------------------------------------------
	// Write data
	//......................................................................
	tec_file << time_current;
	//......................................................................
	// NOD values
	if (getProcessType() == FiniteElement::RANDOM_WALK)
	{
		RandomWalk* PT = (RandomWalk*)PCSGet(FiniteElement::RANDOM_WALK);
		tec_file << PT->leavingParticles << " ";
	}
	int timelevel;
	CRFProcess* m_pcs_out = NULL;

	// fetch geometric entities, especial the associated GEOLIB::Point vector
	if (pcs_vector[0] == NULL) return;

	// 11.06.2012. WW// long msh_node_number(m_msh->GetNODOnPNT(
	//                             static_cast<const GEOLIB::Point*>
	//                             (getGeoObj())));

	// Mass transport
	if (getProcessType() == FiniteElement::MASS_TRANSPORT)
		for (size_t i = 0; i < _nod_value_vector.size(); i++)
		{
			std::string nod_value_name = _nod_value_vector[i];
			for (size_t l = 0; l < pcs_vector.size(); l++)
			{
				m_pcs = pcs_vector[l];
				//				if (m_pcs->pcs_type_name.compare("MASS_TRANSPORT")
				//==
				// 0) { TF
				if (m_pcs->getProcessType() == FiniteElement::MASS_TRANSPORT)
				{
					timelevel = 0;
					for (size_t m = 0; m < m_pcs->nod_val_name_vector.size();
					     m++)
						if (m_pcs->nod_val_name_vector[m].compare(
						        nod_value_name) == 0)
						{
							//							m_pcs_out =
							//PCSGet(pcs_type_name,
							// nod_value_name);
							m_pcs_out = PCSGet(FiniteElement::MASS_TRANSPORT,
							                   nod_value_name);
							if (timelevel == 1)
							{
								int nidx = m_pcs_out->GetNodeValueIndex(
								               nod_value_name) +
								           timelevel;
								tec_file << sep
								         << m_pcs_out->GetNodeValue(
								                msh_node_number, nidx);
							}
							timelevel++;
						}
				}
			}
		}
	else
	{
		double flux_nod, flux_sum = 0.0;
		for (size_t i = 0; i < _nod_value_vector.size(); i++)
		{
			// PCS
			if (!(_nod_value_vector[i].compare("FLUX") == 0))
				m_pcs = GetPCS(_nod_value_vector[i]);
			else
				m_pcs = GetPCS();
			if (!m_pcs)
			{
				cout << "Warning in COutput::NODWritePLYDataTEC - no PCS data"
				     << endl;
				tec_file
				    << "Warning in COutput::NODWritePLYDataTEC - no PCS data"
				    << "\n";
				return;
			}
			//..................................................................
			// PCS
			if (!(_nod_value_vector[i].compare("FLUX") == 0))
			{
				//-----------------------------------------WW
				double val_n =
				    m_pcs->GetNodeValue(msh_node_number, NodeIndex[i]);
				tec_file << sep << val_n;
				m_pcs = GetPCS(_nod_value_vector[i]);
				if (m_pcs->type == 1212 &&
				    (_nod_value_vector[i].find("SATURATION") != string::npos))
					tec_file << 1. - val_n << " ";
				//-----------------------------------------WW
			}
			else
			{
				flux_nod = NODFlux(msh_node_number);
				tec_file << flux_nod << " ";
				// flux_sum += abs(m_pcs->eqs->b[gnode]);
				flux_sum += abs(flux_nod);
				// OK cout << gnode << " " << flux_nod << " " << flux_sum <<
				// endl;
			}
		}
		//....................................................................
		if (dm_pcs && !isCSV)  // WW
		{
			for (size_t i = 0; i < ns; i++)
				ss[i] = dm_pcs->GetNodeValue(msh_node_number, stress_i[i]);
			tec_file << -DeviatoricStress(ss) / 3.0 << " ";
			tec_file << sqrt(3.0 *
			                 TensorMutiplication2(
			                     ss, ss, m_msh->GetCoordinateFlag() / 10) /
			                 2.0) << "  ";
			for (size_t i = 0; i < ns; i++)
				ss[i] = dm_pcs->GetNodeValue(msh_node_number, strain_i[i]);
			DeviatoricStress(ss);
			tec_file << sqrt(
			    3.0 *
			    TensorMutiplication2(ss, ss, m_msh->GetCoordinateFlag() / 10) /
			    2.0);
		}
		// OK411
		for (size_t k = 0; k < mfp_value_vector.size(); k++)
			tec_file
			    << MFPGetNodeValue(
			           msh_node_number, mfp_value_vector[k],
			           atoi(&mfp_value_vector[k][mfp_value_vector[k].size() -
			                                     1]) -
			               1) << " ";  // NB
	}
	tec_file << "\n";
	//----------------------------------------------------------------------
	tec_file.close();  // kg44 close file
}

void COutput::WriteRFOHeader(fstream& rfo_file)
{
	//#0#0#0#1#0.00000000000000e+000#0#3915###########################################
	rfo_file << "#0#0#0#1#";
	rfo_file << _time;
	rfo_file << "#0#";
	rfo_file << OGS_VERSION;
	rfo_file << "###########################################";
	rfo_file << "\n";
}

void COutput::WriteRFONodes(fstream& rfo_file)
{
	// 0 101 100
	rfo_file << 0 << " " << m_msh->nod_vector.size() << " "
	         << m_msh->ele_vector.size() << "\n";
	// 0 0.00000000000000 0.00000000000000 0.00000000000000
	for (size_t i = 0; i < m_msh->nod_vector.size(); i++)
	{
		double const* const pnt_i(m_msh->nod_vector[i]->getData());
		rfo_file << i << " " << pnt_i[0] << " " << pnt_i[1] << " " << pnt_i[2]
		         << " "
		         << "\n";
	}
}

void COutput::WriteRFOElements(fstream& rfo_file)
{
	size_t j;
	MeshLib::CElem* m_ele = NULL;
	// 0 0 -1 line 0 1
	for (long i = 0; i < (long)m_msh->ele_vector.size(); i++)
	{
		m_ele = m_msh->ele_vector[i];
		rfo_file << i << " " << m_ele->GetPatchIndex() << " -1 "
		         << m_ele->GetName() << " ";
		for (j = 0; j < m_ele->GetNodesNumber(false); j++)
			rfo_file << m_ele->getNodeIndices()[j] << " ";
		rfo_file << "\n";
	}
}

void COutput::WriteRFOValues(fstream& rfo_file)
{
	int p, nidx;
	CRFProcess* m_pcs = NULL;
	// 1 2 4
	rfo_file << "1 1 4"
	         << "\n";
	// 2 1 1
	int no_processes = (int)pcs_vector.size();
	rfo_file << no_processes;
	for (p = 0; p < no_processes; p++)
		rfo_file << " 1";
	rfo_file << "\n";
	// PRESSURE1, Pa
	// Names and units
	for (p = 0; p < no_processes; p++)
	{
		m_pcs = pcs_vector[p];
		rfo_file << m_pcs->pcs_primary_function_name[0];
		rfo_file << ", ";
		rfo_file << m_pcs->pcs_primary_function_unit[0];
		rfo_file << "\n";
	}
	// 0 0.00000000000000 0.00000000000000
	// Node values
	for (long i = 0; i < (long)m_msh->nod_vector.size(); i++)
	{
		rfo_file << i;
		for (p = 0; p < no_processes; p++)
		{
			m_pcs = pcs_vector[p];
			nidx =
			    m_pcs->GetNodeValueIndex(m_pcs->pcs_primary_function_name[0]) +
			    1;
			rfo_file << " " << m_pcs->GetNodeValue(i, nidx);
		}
		rfo_file << " "
		         << "\n";
	}
}

void COutput::WriteRFO()
{
	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));
	if (!m_msh)
	{
		cout << "Warning in COutput::WriteRFONodes - no MSH data"
		     << "\n";
		return;
	}
	//--------------------------------------------------------------------
	// File handling
	string rfo_file_name;
	rfo_file_name = file_base_name + "." + "rfo";
	// WW
	if (!_new_file_opened) remove(rfo_file_name.c_str());
	fstream rfo_file(rfo_file_name.data(), ios::app | ios::out);

	rfo_file.setf(ios::scientific, ios::floatfield);
	rfo_file.precision(12);
	if (!rfo_file.good()) return;
	rfo_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	rfo_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	WriteRFOHeader(rfo_file);
	WriteRFONodes(rfo_file);
	WriteRFOElements(rfo_file);
	WriteRFOValues(rfo_file);
	//  RFOWriteELEValues();
	rfo_file.close();  // kg44 close file
}

void COutput::NODWriteSFCDataTEC(int number)
{
	if (_nod_value_vector.size() == 0) return;

	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));
	m_pcs = PCSGet(getProcessType());

	// File handling
	char number_char[3];
	sprintf(number_char, "%i", number);
	string number_string = number_char;
	//	string tec_file_name = pcs_type_name + "_sfc_" + geo_name + "_t"
	//				+ number_string + TEC_FILE_EXTENSION;
	// std::string tec_file_name = convertProcessTypeToString (getProcessType())
	// + "_sfc_" + geo_name + "_t"
	//   + number_string + TEC_FILE_EXTENSION;
	// AB SB Use Model name for output file name
	// std::string tec_file_name = convertProcessTypeToString (getProcessType())
	std::string tec_file_name = file_base_name + "_sfc_" + geo_name + "_t" +
	                            number_string + TEC_FILE_EXTENSION;
	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	// Write header
	// project_title;
	string project_title_string = "Profile at surface";
	tec_file << " TITLE = \"" << project_title_string << "\""
	         << "\n";
	tec_file << " VARIABLES = \"X\",\"Y\",\"Z\",";
	for (size_t k = 0; k < _nod_value_vector.size(); k++)
		tec_file << _nod_value_vector[k] << ",";
	tec_file << "\n";
	// , I=" << NodeListLength << ", J=1, K=1, F=POINT" << "\n";
	tec_file << " ZONE T=\"TIME=" << _time << "\""
	         << "\n";
	//--------------------------------------------------------------------
	// Write data
	std::vector<long> nodes_vector;
	Surface* m_sfc = NULL;
	m_sfc = GEOGetSFCByName(geo_name);  // CC
	if (m_sfc)
	{
		m_msh->GetNODOnSFC(m_sfc, nodes_vector);
		// for (size_t i = 0; i < m_msh->nod_vector.size(); i++)
		for (size_t i = 0; i < nodes_vector.size(); i++)  // AB SB
		{
			double const* const pnt_i(
			    m_msh->nod_vector[nodes_vector[i]]->getData());
			tec_file << pnt_i[0] << " ";
			tec_file << pnt_i[1] << " ";
			tec_file << pnt_i[2] << " ";
			for (size_t k = 0; k < _nod_value_vector.size(); k++)
			{
				m_pcs = PCSGet(_nod_value_vector[k], true);  // AB SB
				int nidx = m_pcs->GetNodeValueIndex(_nod_value_vector[k]) + 1;
				tec_file << m_pcs->GetNodeValue(nodes_vector[i], nidx) << " ";
			}
			tec_file << "\n";
		}
	}
	else
		tec_file << "Error in NODWritePLYDataTEC: polyline " << geo_name
		         << " not found"
		         << "\n";
	tec_file.close();  // kg44 close file
}

/**************************************************************************
   FEMLib-Method:
   12/2005 OK Implementation
   04/2006 CMCD no mesh option & flux weighting
   last modification:
**************************************************************************/
void COutput::NODWriteSFCAverageDataTEC(double time_current,
                                        int time_step_number)
{
	bool no_pcs = false;
	double dtemp;
	vector<long> sfc_nodes_vector;
	double node_flux = 0.0;
	int idx = -1;
	double t_flux = 0.0;
	double node_conc = 0.0;
	CRFProcess* m_pcs_gw(PCSGet(FiniteElement::GROUNDWATER_FLOW));
	if (!m_pcs_gw) PCSGet(FiniteElement::LIQUID_FLOW);
	//--------------------------------------------------------------------
	// Tests
	Surface* m_sfc = NULL;
	m_sfc = GEOGetSFCByName(geo_name);
	if (!m_sfc)
	{
		cout << "Warning in COutput::NODWriteSFCAverageDataTEC - no GEO data"
		     << endl;
		return;
	}
	//	CFEMesh* m_msh = NULL;
	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));
	if (!m_msh)
	{
		cout << "Warning in COutput::NODWriteSFCAverageDataTEC - no MSH data"
		     << endl;
		return;
	}
	CRFProcess* m_pcs(PCSGet(getProcessType()));
	if (!m_pcs) no_pcs = true;
	// cout << "Warning in COutput::NODWriteSFCAverageDataTEC - no PCS data" <<
	// endl;
	// return;
	//--------------------------------------------------------------------
	// File handling
	string tec_file_name = file_base_name + "_TBC_" + getGeoTypeAsString() +
	                       "_" + geo_name + TEC_FILE_EXTENSION;
	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	// Write header
	if (time_step_number == 0)  // WW Old:  if(time_step_number==1)
	{
		// project_title;
		string project_title_string = "Time curve at surface";
		tec_file << " TITLE = \"" << project_title_string << "\""
		         << "\n";
		tec_file << " VARIABLES = Time ";
		for (size_t i = 0; i < _nod_value_vector.size(); i++)
			tec_file << _nod_value_vector[i] << " ";
		tec_file << "\n";
		//, I=" << anz_zeitschritte << ", J=1, K=1, F=POINT" << "\n";
		tec_file << " ZONE T=\"SFC=" << geo_name << "\""
		         << "\n";
	}
	//--------------------------------------------------------------------
	// node_value_index_vector

	std::vector<int> node_value_index_vector(_nod_value_vector.size());
	//	int itemp;
	if (m_pcs)
		for (size_t i = 0; i < _nod_value_vector.size(); i++)
		{
			node_value_index_vector[i] =
			    m_pcs->GetNodeValueIndex(_nod_value_vector[i]) + 1;
			//			itemp = node_value_index_vector[i];
			for (size_t n_pv = 0; n_pv < m_pcs->GetPrimaryVNumber(); n_pv++)
				if (_nod_value_vector[i].compare(
				        m_pcs->pcs_primary_function_name[n_pv]) == 0)
				{
					node_value_index_vector[i]++;
					break;
				}
		}
	//--------------------------------------------------------------------
	// Write data
	if (no_pcs)
	{
		tec_file << time_current << " ";
		for (size_t i = 0; i < _nod_value_vector.size(); i++)
		{
			// Specified currently for MASS_TRANSPORT only.
			m_pcs = PCSGet(FiniteElement::MASS_TRANSPORT, _nod_value_vector[i]);
			node_value_index_vector[i] =
			    m_pcs->GetNodeValueIndex(_nod_value_vector[i]) + 1;
			m_pcs->m_msh->GetNODOnSFC(m_sfc, sfc_nodes_vector);
			dtemp = 0.0;
			t_flux = 0.0;
			for (size_t j = 0; j < sfc_nodes_vector.size(); j++)
			{
				idx = m_pcs_gw->GetNodeValueIndex("FLUX");
				node_flux =
				    abs(m_pcs_gw->GetNodeValue(sfc_nodes_vector[j], idx));
				node_conc = m_pcs->GetNodeValue(sfc_nodes_vector[j],
				                                node_value_index_vector[i]);
				dtemp += (node_flux * node_conc);
				t_flux += node_flux;
			}
			dtemp /= t_flux;
			tec_file << dtemp << " ";
		}
		tec_file << "\n";
	}
	else
	{
		m_msh->GetNODOnSFC(m_sfc, sfc_nodes_vector);
		idx = m_pcs_gw->GetNodeValueIndex("FLUX");
		tec_file << time_current << " ";
		dtemp = 0.0;
		t_flux = 0.0;
		for (size_t i = 0; i < _nod_value_vector.size(); i++)
		{
			dtemp = 0.0;
			for (size_t j = 0; j < sfc_nodes_vector.size(); j++)
			{
				node_flux =
				    abs(m_pcs_gw->GetNodeValue(sfc_nodes_vector[j], idx));
				node_conc = m_pcs->GetNodeValue(sfc_nodes_vector[j],
				                                node_value_index_vector[i]);
				dtemp += (node_flux * node_conc);
				t_flux += node_flux;
			}
			dtemp /= t_flux;
			tec_file << dtemp << " ";
		}
		tec_file << "\n";
	}
	tec_file.close();  // kg44 close file
}

void COutput::GetNodeIndexVector(vector<int>& NodeIndex)
{
	CRFProcess* pcs = NULL;
	const size_t nName = _nod_value_vector.size();
	if (getProcessType() != FiniteElement::INVALID_PROCESS)
	{
		pcs = PCSGet(getProcessType());
		for (size_t k = 0; k < nName; k++)
		{
			if (getProcessType() == FiniteElement::MASS_TRANSPORT)
				pcs = PCSGet(getProcessType(), _nod_value_vector[k]);
			if (!pcs)
			{
				cout << "Warning in COutput::GetNodeIndexVector - no PCS data: "
				     << _nod_value_vector[k] << endl;
				return;
			}
			NodeIndex[k] = pcs->GetNodeValueIndex(_nod_value_vector[k],
			                                      true);  // JT latest
		}
	}
	else if (msh_type_name.size() > 0)
	{
		pcs = PCSGet(msh_type_name);
		if (!pcs)
		{
			cout << "Warning in COutput::GetNodeIndexVector - no PCS data"
			     << endl;
			return;
		}
		for (size_t k = 0; k < nName; k++)
		{
			NodeIndex[k] = pcs->GetNodeValueIndex(_nod_value_vector[k],
			                                      true);  // JT latest
		}
	}
	else if (fem_msh_vector.size() == 1)
	{
		bool bdummy = true;
		for (size_t k = 0; k < nName; k++)
		{
			pcs = PCSGet(_nod_value_vector[k], bdummy);
			if (!pcs)
			{
				cout << "Warning in COutput::GetNodeIndexVector - no PCS data: "
				     << _nod_value_vector[k] << endl;
				return;
			}
			NodeIndex[k] = pcs->GetNodeValueIndex(_nod_value_vector[k],
			                                      true);  // JT latest
		}
	}
}

CRFProcess* COutput::GetPCS(const string& var_name)
{
	CRFProcess* m_pcs = NULL;
	if (getProcessType() != FiniteElement::INVALID_PROCESS)
		m_pcs = PCSGet(getProcessType());
	else if (msh_type_name.size() > 0)
		m_pcs = PCSGet(msh_type_name);
	if (!m_pcs) m_pcs = PCSGet(var_name, true);
	return m_pcs;
}

// 09/2010 TF
CRFProcess* COutput::GetPCS()
{
	if (getProcessType() != FiniteElement::INVALID_PROCESS)
	{
		if (getProcess() != NULL)
			return getProcess();
		else
			return PCSGet(getProcessType());
	}
	else
	{
		CRFProcess* pcs(NULL);
		if (msh_type_name.size() > 0) pcs = PCSGet(msh_type_name);
		//	  if(!pcs)
		//		pcs = PCSGet(var_name,true);
		return pcs;
	}
}

CRFProcess* COutput::GetPCS_ELE(const string& var_name)
{
	string pcs_var_name;
	CRFProcess* m_pcs = NULL;
	//----------------------------------------------------------------------
	//  if(pcs_type_name.size()>0)
	//    m_pcs = PCSGet(pcs_type_name);
	if (getProcessType() != FiniteElement::INVALID_PROCESS)
		m_pcs = PCSGet(getProcessType());
	else if (msh_type_name.size() > 0)
		m_pcs = PCSGet(msh_type_name);

	if (!m_pcs)
		for (size_t i = 0; i < pcs_vector.size(); i++)
		{
			m_pcs = pcs_vector[i];
			for (int j = 0; j < m_pcs->pcs_number_of_evals; j++)
			{
				pcs_var_name = m_pcs->pcs_eval_name[j];
				if (pcs_var_name.compare(var_name) == 0) return m_pcs;
			}
		}
	return m_pcs;
}

void COutput::GetELEValuesIndexVector(vector<int>& ele_value_index_vector)
{
	if (_ele_value_vector[0].size() == 0) return;
	CRFProcess* m_pcs = NULL;

	// CB THMBM
	// m_pcs = GetPCS_ELE(_ele_value_vector[0]);   // CB this is buggy: not all
	// ele vals are defined with the same (or any) process
	for (size_t i = 0; i < _ele_value_vector.size(); i++)
	{
		m_pcs = GetPCS_ELE(_ele_value_vector[i]);  // CB
		ele_value_index_vector[i] =
		    m_pcs->GetElementValueIndex(_ele_value_vector[i]);
	}
}

/**************************************************************************
   FEMLib-Method:
   Programing:
   01/2006 OK Implementation
   07/2010 TF substituted GEOGetPLYByName, renamed local variables for
CGLPolyline,
         CFEMesh and CRFProcess
         (conflicting with class attributes that have the same name)
**************************************************************************/
void COutput::SetNODFluxAtPLY()
{
	// Tests
	//  CGLPolyline* ply = GEOGetPLYByName(geo_name);
	//	CGLPolyline* ply = polyline_vector[getGeoObjIdx()];
	//	if (!ply) {
	//		cout << "Warning in COutput::SetNODFluxAtPLY() - no PLY data" <<
	// endl;
	//		return;
	//	}

	//	CFEMesh* msh = GetMSH();
	if (!m_msh)
	{
		cout << "Warning in COutput::SetNODFluxAtPLY() - no MSH data" << endl;
		return;
	}

	CRFProcess* pcs = GetPCS("FLUX");
	if (!pcs)
	{
		cout << "Warning in COutput::SetNODFluxAtPLY() - no PCS data" << endl;
		return;
	}

	std::vector<long> nodes_vector;
	//	msh->GetNODOnPLY(ply, nodes_vector);
	m_msh->GetNODOnPLY(static_cast<const GEOLIB::Polyline*>(getGeoObj()),
	                   nodes_vector);
	std::vector<double> node_value_vector;
	node_value_vector.resize(nodes_vector.size());
	int nidx = pcs->GetNodeValueIndex("FLUX");
	for (size_t i = 0; i < nodes_vector.size(); i++)
		node_value_vector[i] = pcs->GetNodeValue(nodes_vector[i], nidx);
	//----------------------------------------------------------------------
	// m_st->FaceIntegration(m_pcs,nodes_vector,node_value_vector);
}

void COutput::ELEWriteSFC_TEC()
{
	//----------------------------------------------------------------------
	if (_ele_value_vector.size() == 0) return;
	//----------------------------------------------------------------------
	// File handling
	//......................................................................
	std::string tec_file_name = file_base_name + "_surface" + "_ele";
	addInfoToFileName(tec_file_name, false, true, true);
	//  if(pcs_type_name.size()>1) // PCS
	//    tec_file_name += "_" + msh_type_name;
	//  if(msh_type_name.size()>1) // MSH
	//    tec_file_name += "_" + msh_type_name;
	//  tec_file_name += TEC_FILE_EXTENSION;

	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	//......................................................................
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	vector<long> tmp_ele_sfc_vector;
	tmp_ele_sfc_vector.clear();
	//--------------------------------------------------------------------
	ELEWriteSFC_TECHeader(tec_file);
	ELEWriteSFC_TECData(tec_file);
	//--------------------------------------------------------------------
	tec_file.close();  // kg44 close file
}

void COutput::ELEWriteSFC_TECHeader(fstream& tec_file)
{
	// Write Header I: variables
	tec_file << "VARIABLES = \"X\",\"Y\",\"Z\"";
	for (size_t i = 0; i < _ele_value_vector.size(); i++)
		tec_file << "," << _ele_value_vector[i];
	tec_file << "\n";
	//--------------------------------------------------------------------
	// Write Header II: zone
	tec_file << "ZONE T=\"";
	tec_file << _time << "s\", ";
	tec_file << "I=" << (long)m_msh->ele_vector.size() << ", ";
	tec_file << "F=POINT"
	         << ", ";
	tec_file << "C=BLACK";
	tec_file << "\n";
}

void COutput::ELEWriteSFC_TECData(fstream& tec_file)
{
	tec_file << "COutput::ELEWriteSFC_TECData - implementation not finished"
	         << "\n";

	/* // Make it as comment to avoid compilation warnings. 18.082011 WW
	   long i;
	   int j;
	   MeshLib::CElem* m_ele = NULL;
	   MeshLib::CElem* m_ele_neighbor = NULL;
	   double v[3];
	   CRFProcess* m_pcs = NULL;
	   double v_n;
	   //--------------------------------------------------------------------
	   m_pcs = pcs_vector[0]; //GetPCS_ELE(ele_value_vector[0]);
	   int nidx[3];
	   nidx[0] = m_pcs->GetElementValueIndex("VELOCITY1_X");
	   nidx[1] = m_pcs->GetElementValueIndex("VELOCITY1_Y");
	   nidx[2] = m_pcs->GetElementValueIndex("VELOCITY1_Z");
	   //--------------------------------------------------------------------
	   for(i=0l;i<(long)m_msh->ele_vector.size();i++)
	   {
	   m_ele = m_msh->ele_vector[i];
	   for(j=0;j<m_ele->GetFacesNumber();j++)
	   {
	      m_ele_neighbor = m_ele->GetNeighbor(j);
	      if((m_ele->GetDimension() - m_ele_neighbor->GetDimension())==1)
	      {
	         v[0] = m_pcs->GetElementValue(m_ele->GetIndex(),nidx[0]);
	         v[1] = m_pcs->GetElementValue(m_ele->GetIndex(),nidx[1]);
	         v[2] = m_pcs->GetElementValue(m_ele->GetIndex(),nidx[2]);
	         m_ele_neighbor->SetNormalVector();

	         v_n = v[0]*m_ele_neighbor->normal_vector[0] \
	 + v[1]*m_ele_neighbor->normal_vector[1] \
	 + v[2]*m_ele_neighbor->normal_vector[2];

	      }
	   }
	   }
	 */
	//--------------------------------------------------------------------
}

/**************************************************************************
   FEMLib-Method:
   08/2006 OK Implementation
   modification:
   05/2010 TF if geo_type_name[3] == 'N' (case point) only a pointer to the
   CGLPoint is fetched, but the pointer is never used
   07/2010 TF substituted GEOGetPLYByName
   09/2010 TF substituted pcs_type_name
**************************************************************************/
void COutput::CalcELEFluxes()
{
	double Test[5];

	const FiniteElement::ProcessType pcs_type(getProcessType());
	if (pcs_type == FiniteElement::INVALID_PROCESS)  // WW moved it here.

		// WW cout << "Warning in COutput::CalcELEFluxes(): no PCS data" <<
		// endl;
		return;

	CRFProcess* pcs = PCSGet(getProcessType());
	// BG 04/2011: MASS_TRANSPORT added to get MASS FLUX for Polylines
	// cout << pcs->Tim->step_current << endl;
	if (isDeformationProcess(pcs_type) ||
	    (!isFlowProcess(pcs_type) &&
	     (pcs_type != FiniteElement::MASS_TRANSPORT)))
		return;

	//----------------------------------------------------------------------
	switch (getGeoType())
	{
		case GEOLIB::POLYLINE:
		{
			//		CGLPolyline* ply = GEOGetPLYByName(geo_name);
			//		if (!ply)
			//			std::cout << "Warning in COutput::CalcELEFluxes - no GEO
			// data" << "\n";

			// BG 04/2011: ELEWritePLY_TEC does not work for MASS_TRANSPORT
			// because there is no flux considered
			if (pcs_type != FiniteElement::MASS_TRANSPORT)
			{
				double f_n_sum = 0.0;
				double* PhaseFlux(new double[2]);
				std::string Header[2];
				int dimension = 2;
				Header[0] = "q_Phase1";
				Header[1] = "q_Phase2";

				pcs->CalcELEFluxes(
				    static_cast<const GEOLIB::Polyline*>(getGeoObj()),
				    PhaseFlux);
				if (pcs_type == FiniteElement::GROUNDWATER_FLOW)
				{
					ELEWritePLY_TEC();
					f_n_sum = PhaseFlux[0];
					TIMValue_TEC(f_n_sum);
				}
				if (pcs_type == FiniteElement::MULTI_PHASE_FLOW)
				{
					Test[0] = PhaseFlux[0];
					Test[1] = PhaseFlux[1];
					TIMValues_TEC(Test, Header, dimension);
				}
				delete[] PhaseFlux;
			}
			// BG, Output for Massflux added
			else
			{
				double* MassFlux(new double[5]);
				std::string Header[5];
				int dimension = 5;
				Header[0] = "AdvectiveMassFlux";
				Header[1] = "DispersiveMassFlux";
				Header[2] = "DiffusiveMassFlux";
				Header[3] = "TotalMassFlux";
				Header[4] = "TotalMass_sum";

				pcs->CalcELEMassFluxes(
				    static_cast<const GEOLIB::Polyline*>(getGeoObj()), geo_name,
				    MassFlux);
				Test[0] = MassFlux[0];
				Test[1] = MassFlux[1];
				Test[2] = MassFlux[2];
				Test[3] = MassFlux[3];
				Test[4] = MassFlux[4];
				TIMValues_TEC(Test, Header, dimension);
				delete[] MassFlux;
			}

			// double f_n_sum = 0.0;
			//		f_n_sum = pcs->CalcELEFluxes(ply); // TF
			// f_n_sum = pcs->CalcELEFluxes(static_cast<const GEOLIB::Polyline*>
			// (getGeoObj()));

			// ELEWritePLY_TEC();
			// BUGFIX_4402_OK_1
			// TIMValue_TEC(f_n_sum);
			break;
		}
		case GEOLIB::SURFACE:
		{
			//		Surface* m_sfc = GEOGetSFCByName(geo_name);
			//		pcs->CalcELEFluxes(m_sfc);
			break;
		}
		case GEOLIB::VOLUME:
		{
			//		CGLVolume* m_vol = GEOGetVOL(geo_name);
			//		pcs->CalcELEFluxes(m_vol);
			break;
		}
		case GEOLIB::GEODOMAIN:  // domAin
			// pcs->CalcELEFluxes(m_dom);
			break;
		default:
			cout << "Warning in COutput::CalcELEFluxes(): no GEO type data"
			     << endl;
			break;
	}

	// WW   pcs->CalcELEFluxes(ply) changed 'mark' of elements
	for (size_t i = 0; i < fem_msh_vector.size(); i++)
		for (size_t j = 0; j < fem_msh_vector[i]->ele_vector.size(); j++)
			fem_msh_vector[i]->ele_vector[j]->MarkingAll(true);
}

void COutput::ELEWritePLY_TEC()
{
	//----------------------------------------------------------------------
	if (_ele_value_vector.size() == 0) return;
	//----------------------------------------------------------------------
	// File handling
	//......................................................................
	string tec_file_name = file_base_name;  // + "_ply" + "_ele";
	tec_file_name += "_" + getGeoTypeAsString();
	tec_file_name += "_" + geo_name;
	tec_file_name += "_ELE";
	//  if(pcs_type_name.size()>1) // PCS
	//    tec_file_name += "_" + pcs_type_name;
	if (getProcessType() != FiniteElement::INVALID_PROCESS)  // PCS
		tec_file_name += "_" + convertProcessTypeToString(getProcessType());

	if (msh_type_name.size() > 1)  // MSH
		tec_file_name += "_" + msh_type_name;
	tec_file_name += TEC_FILE_EXTENSION;
	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	//......................................................................
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	vector<long> tmp_ele_ply_vector;
	tmp_ele_ply_vector.clear();
	//--------------------------------------------------------------------
	ELEWritePLY_TECHeader(tec_file);
	ELEWritePLY_TECData(tec_file);
	//--------------------------------------------------------------------

	tec_file.close();  // kg44 close file
}

void COutput::ELEWritePLY_TECHeader(fstream& tec_file)
{
	// Write Header I: variables
	tec_file << "VARIABLES = \"X\",\"Y\",\"Z\"";
	for (size_t i = 0; i < _ele_value_vector.size(); i++)
		tec_file << "," << _ele_value_vector[i];
	tec_file << "\n";

	// Write Header II: zone
	tec_file << "ZONE T=\"";
	tec_file << _time << "s\", ";
	tec_file << "\n";
}

/**************************************************************************
   FEMLib-Method:
   06/2006 OK Implementation
   07/2010 TF substituted GEOGetPLYByName
   10/2010 TF changed access to process type
**************************************************************************/
void COutput::ELEWritePLY_TECData(fstream& tec_file)
{
	//	CRFProcess* pcs = PCSGet(pcs_type_name);
	CRFProcess* pcs = PCSGet(getProcessType());
	int f_eidx[3];
	f_eidx[0] = pcs->GetElementValueIndex("FLUX_X");
	f_eidx[1] = pcs->GetElementValueIndex("FLUX_Y");
	f_eidx[2] = pcs->GetElementValueIndex("FLUX_Z");
	for (size_t i = 0; i < 3; i++)
		if (f_eidx[i] < 0)
		{
			cout << "Fatal error in "
			        "CRFProcess::CalcELEFluxes(CGLPolyline*m_ply) - abort";
			abort();
		}
	int v_eidx[3];
	CRFProcess* m_pcs_flow = NULL;

	//	if (pcs->pcs_type_name.find("FLOW") != string::npos) {
	if (isFlowProcess(pcs->getProcessType()))
		m_pcs_flow = pcs;
	else
		m_pcs_flow = PCSGet(FiniteElement::GROUNDWATER_FLOW);

	v_eidx[0] = m_pcs_flow->GetElementValueIndex("VELOCITY1_X");
	v_eidx[1] = m_pcs_flow->GetElementValueIndex("VELOCITY1_Y");
	v_eidx[2] = m_pcs_flow->GetElementValueIndex("VELOCITY1_Z");
	for (size_t i = 0; i < 3; i++)
		if (v_eidx[i] < 0)
		{
			cout << "Fatal error in "
			        "CRFProcess::CalcELEFluxes(CGLPolyline*m_ply) - abort";
			abort();
		}

	//  CGLPolyline* m_ply = GEOGetPLYByName(geo_name);

	// Get elements at GEO
	//	vector<long> ele_vector_at_geo;
	//	m_msh->GetELEOnPLY(m_ply, ele_vector_at_geo);
	std::vector<size_t> ele_vector_at_geo;
	m_msh->GetELEOnPLY(static_cast<const GEOLIB::Polyline*>(getGeoObj()),
	                   ele_vector_at_geo, false);

	// helper variables
	Math_Group::vec<MeshLib::CEdge*> ele_edges_vector(15);
	Math_Group::vec<MeshLib::CNode*> edge_nodes(3);
	double edge_mid_vector[3] = {0.0, 0.0, 0.0};

	for (size_t i = 0; i < ele_vector_at_geo.size(); i++)
	{
		MeshLib::CElem* m_ele = m_msh->ele_vector[ele_vector_at_geo[i]];
		// x,y,z
		m_ele->GetEdges(ele_edges_vector);
		for (size_t j = 0; j < m_ele->GetEdgesNumber(); j++)
		{
			MeshLib::CEdge* m_edg = ele_edges_vector[j];
			if (m_edg->GetMark())
			{
				m_edg->GetNodes(edge_nodes);
				double const* const pnt0(edge_nodes[0]->getData());
				double const* const pnt1(edge_nodes[1]->getData());
				edge_mid_vector[0] = 0.5 * (pnt1[0] + pnt0[0]);
				edge_mid_vector[1] = 0.5 * (pnt1[1] + pnt0[1]);
				edge_mid_vector[2] = 0.5 * (pnt1[2] + pnt0[2]);
			}
		}
		tec_file << edge_mid_vector[0] << " " << edge_mid_vector[1] << " "
		         << edge_mid_vector[2];
		// ele vector values
		tec_file << " "
		         << m_pcs_flow->GetElementValue(m_ele->GetIndex(), v_eidx[0]);
		tec_file << " "
		         << m_pcs_flow->GetElementValue(m_ele->GetIndex(), v_eidx[1]);
		tec_file << " "
		         << m_pcs_flow->GetElementValue(m_ele->GetIndex(), v_eidx[2]);
		tec_file << " " << pcs->GetElementValue(m_ele->GetIndex(), f_eidx[0]);
		tec_file << " " << pcs->GetElementValue(m_ele->GetIndex(), f_eidx[1]);
		tec_file << " " << pcs->GetElementValue(m_ele->GetIndex(), f_eidx[2]);
		tec_file << "\n";
	}
}

void COutput::TIMValue_TEC(double tim_value)
{
	//----------------------------------------------------------------------
	// File handling
	//......................................................................
	fstream tec_file;
	string tec_file_name = file_base_name;  // + "_ply" + "_ele";
	tec_file_name += "_" + getGeoTypeAsString();
	tec_file_name += "_" + geo_name;
	tec_file_name += "_TIM";
	//  if(pcs_type_name.size()>1) // PCS
	//    tec_file_name += "_" + pcs_type_name;
	if (getProcessType() != FiniteElement::INVALID_PROCESS)  // PCS
		tec_file_name += "_" + convertProcessTypeToString(getProcessType());
	if (msh_type_name.size() > 1)  // MSH
		tec_file_name += "_" + msh_type_name;
	tec_file_name += TEC_FILE_EXTENSION;
	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	//......................................................................
	if (aktueller_zeitschritt == 0)  // BG:04/2011 deletes the content of the
		                             // file at the start of the simulation
		tec_file.open(tec_file_name.data(), ios::trunc | ios::out);
	else
		tec_file.open(tec_file_name.data(), ios::app | ios::out);

	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	// Write Header I: variables
	if (aktueller_zeitschritt == 0)  // BG:04/2011 bevor it was timestep 1
	{
		tec_file << "VARIABLES = \"Time\",\"Value\"";
		tec_file << "\n";
		//--------------------------------------------------------------------
		// Write Header II: zone
		tec_file << "ZONE T=";
		tec_file << geo_name;
		tec_file << "\n";
	}
	//--------------------------------------------------------------------
	tec_file << aktuelle_zeit << " " << tim_value << "\n";
	//--------------------------------------------------------------------
	tec_file.close();  // kg44 close file
}

/*-------------------------------------------------------------------------
   GeoSys - Function: TIMValues_TEC
   Task: Can write several values over time
   Return: nothing
   Programming: 10/2011 BG
   Modification:
 -------------------------------------------------------------------------*/
void COutput::TIMValues_TEC(double tim_value[5], std::string* header,
                            int dimension)
{
	double j[10];

	for (int i = 0; i < dimension; i++)
		j[i] = tim_value[i];

	//----------------------------------------------------------------------
	// File handling
	//......................................................................
	fstream tec_file;
	string tec_file_name = file_base_name;  // + "_ply" + "_ele";
	tec_file_name += "_" + getGeoTypeAsString();
	tec_file_name += "_" + geo_name;
	tec_file_name += "_TIM";
	//  if(pcs_type_name.size()>1) // PCS
	//    tec_file_name += "_" + pcs_type_name;
	if (getProcessType() != FiniteElement::INVALID_PROCESS)  // PCS
		tec_file_name += "_" + convertProcessTypeToString(getProcessType());
	if (msh_type_name.size() > 1)  // MSH
		tec_file_name += "_" + msh_type_name;
	tec_file_name += TEC_FILE_EXTENSION;

	if (!_new_file_opened) remove(tec_file_name.c_str());  // WW
	//......................................................................
	if (aktueller_zeitschritt == 0)  // BG:04/2011 deletes the content of the
		                             // file at the start of the simulation
		tec_file.open(tec_file_name.data(), ios::trunc | ios::out);
	else
		tec_file.open(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
#ifdef SUPERCOMPUTER
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	// Write Header I: variables
	if (aktueller_zeitschritt == 0)  // BG:04/2011 bevor it was timestep 1
	{
		tec_file << "VARIABLES = \"Time\"";
		for (int i = 0; i < dimension; i++)
			tec_file << ",\"" << header[i] << "\"";
		tec_file << "\n";
		//--------------------------------------------------------------------
		// Write Header II: zone
		tec_file << "ZONE T=";
		tec_file << geo_name;
		tec_file << "\n";
	}
	//--------------------------------------------------------------------
	tec_file << aktuelle_zeit;
	for (int i = 0; i < dimension; i++)
		tec_file << " " << j[i];
	tec_file << "\n";
	//--------------------------------------------------------------------
	tec_file.close();  // kg44 close file
}

double COutput::NODFlux(long /*nod_number*/)
{
	/*
	   cout << gnode << " " \
	    << m_pcs->GetNodeValue(gnode,NodeIndex[k]) << end
	   flux_sum += m_pcs->GetNodeValue(gnode,NodeIndex[k]);
	 */
	// All elements at node //OK
	//#if defined (USE_PETSC) // || defined (other parallel solver lib). 04.2012
	// WW
	//	return 0;
	//#elif NEW_EQS                                 //WW. 07.11.2008
	//	return 0.;                            //To do:
	// m_pcs->eqs_new->getRHS()[nod_number];
	//#else
	//	// Element nodal RHS contributions
	//	m_pcs->getEQSPointer()->b[nod_number] = 0.0;
	//	MeshLib::CNode* nod = m_msh->nod_vector[nod_number];
	//	m_pcs->AssembleParabolicEquationRHSVector(nod);
	//	return m_pcs->getEQSPointer()->b[nod_number];
	//#endif
	return 0;
}

void COutput::NODWriteLAYDataTEC(int time_step_number)
{
	// Tests
	const size_t nName(_nod_value_vector.size());
	if (nName == 0) return;
	std::vector<int> NodeIndex(nName);

	// PCS
	CRFProcess* m_pcs = PCSGet(getProcessType());
	if (!m_pcs) return;
	for (size_t k = 0; k < nName; k++)
		NodeIndex[k] = m_pcs->GetNodeValueIndex(_nod_value_vector[k]);

	// MSH
	//	m_msh = GetMSH();
	if (!m_msh)
	{
		cout << "Warning in COutput::NODWriteLAYDataTEC() - no MSH data"
		     << endl;
		return;
	}

	// File name handling
	char char_time_step_number[10];
	sprintf(char_time_step_number, "%i", time_step_number);
	string tec_file_name(file_base_name);
	tec_file_name += "_layer_";
	tec_file_name += char_time_step_number;
	tec_file_name += TEC_FILE_EXTENSION;
	fstream tec_file(tec_file_name.data(), ios::trunc | ios::out);

	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
#ifdef SUPERCOMPUTER
	//
	// kg44 buffer the output
	char mybuffer[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
	tec_file.rdbuf()->pubsetbuf(mybuffer, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
//
#endif
	//--------------------------------------------------------------------
	// Write Header I: variables
	tec_file << "VARIABLES = X,Y,Z,N";
	for (size_t k = 0; k < nName; k++)
		tec_file << "," << _nod_value_vector[k] << " ";
	tec_file << "\n";

	long j;
	long no_per_layer =
	    m_msh->GetNodesNumber(false) / (m_msh->getNumberOfMeshLayers() + 1);
	long jl;
	for (size_t l = 0; l < m_msh->getNumberOfMeshLayers() + 1; l++)
	{
		//--------------------------------------------------------------------
		tec_file << "ZONE T=LAYER" << l << "\n";
		//--------------------------------------------------------------------
		for (j = 0l; j < no_per_layer; j++)
		{
			jl = j + j * m_msh->getNumberOfMeshLayers() + l;
			//..................................................................
			// XYZ
			double const* const pnt(m_msh->nod_vector[jl]->getData());
			tec_file << pnt[0] << " ";
			tec_file << pnt[1] << " ";
			tec_file << pnt[2] << " ";
			tec_file << jl << " ";
			//..................................................................
			for (size_t k = 0; k < nName; k++)
				tec_file << m_pcs->GetNodeValue(
				                m_msh->nod_vector[jl]->GetIndex(), NodeIndex[k])
				         << " ";
			tec_file << "\n";
		}
	}
	tec_file.close();  // kg44 close file
}

/**************************************************************************
   FEMLib-Method:
   Task: Write PCON data for ChemApp output
   Programing:
   08/2008 MX Implementation
**************************************************************************/
void COutput::PCONWriteDOMDataTEC()
{
	int te = 0;
	string eleType;
	string tec_file_name;

	//----------------------------------------------------------------------
	// Tests
	if (_pcon_value_vector.size() == 0) return;
	//......................................................................
	// MSH
	// m_msh = MeshLib::FEMGet(pcs_type_name);
	//  m_msh = GetMSH();
	if (!m_msh)
	{
		cout << "Warning in COutput::NODWriteDOMDataTEC() - no MSH data"
		     << endl;
		return;
	}
	//======================================================================
	vector<int> mesh_type_list;  // NW
	if (m_msh->getNumberOfLines() > 0) mesh_type_list.push_back(1);
	if (m_msh->getNumberOfQuads() > 0) mesh_type_list.push_back(2);
	if (m_msh->getNumberOfHexs() > 0) mesh_type_list.push_back(3);
	if (m_msh->getNumberOfTris() > 0) mesh_type_list.push_back(4);
	if (m_msh->getNumberOfTets() > 0) mesh_type_list.push_back(5);
	if (m_msh->getNumberOfPrisms() > 0) mesh_type_list.push_back(6);

	// Output files for each mesh type
	// NW
	for (int i = 0; i < (int)mesh_type_list.size(); i++)
	{
		te = mesh_type_list[i];
		//----------------------------------------------------------------------
		// File name handling
		tec_file_name = file_base_name + "_" + "domain_PCON";
		if (msh_type_name.size() > 0)  // MultiMSH
			tec_file_name += "_" + msh_type_name;
		//  if(pcs_type_name.size()>0) // PCS
		//    tec_file_name += "_" + pcs_type_name;
		if (getProcessType() != FiniteElement::INVALID_PROCESS)  // PCS
			tec_file_name += "_" + convertProcessTypeToString(getProcessType());
		//======================================================================
		switch (te)  // NW
		{
			case 1:
				tec_file_name += "_line";
				eleType = "QUADRILATERAL";
				break;
			case 2:
				tec_file_name += "_quad";
				eleType = "QUADRILATERAL";
				break;
			case 3:
				tec_file_name += "_hex";
				eleType = "BRICK";
				break;
			case 4:
				tec_file_name += "_tri";
				eleType = "QUADRILATERAL";
				break;
			case 5:
				tec_file_name += "_tet";
				eleType = "TETRAHEDRON";
				break;
			case 6:
				tec_file_name += "_pris";
				eleType = "BRICK";
				break;
		}
/*
   if(m_msh->msh_no_line>0)
   {
      tec_file_name += "_line";
      eleType = "QUADRILATERAL";
     te=1;
   }
   else if (m_msh->msh_no_quad>0)
   {
      tec_file_name += "_quad";
      eleType = "QUADRILATERAL";
   te=2;
   }
   else if (m_msh->msh_no_hexs>0)
   {
   tec_file_name += "_hex";
   eleType = "BRICK";
   te=3;
   }
   else if (m_msh->msh_no_tris>0)
   {
   tec_file_name += "_tri";
   //???Who was this eleType = "TRIANGLE";
   eleType = "QUADRILATERAL";
   te=4;
   }
   else if (m_msh->msh_no_tets>0)
   {
   tec_file_name += "_tet";
   eleType = "TETRAHEDRON";
   te=5;
   }
   else if (m_msh->msh_no_pris>0)
   {
   tec_file_name += "_pris";
   eleType = "BRICK";
   te=6;
   }
 */
		tec_file_name += TEC_FILE_EXTENSION;
		// WW
		if (!_new_file_opened) remove(tec_file_name.c_str());
		fstream tec_file(tec_file_name.data(), ios::app | ios::out);
		tec_file.setf(ios::scientific, ios::floatfield);
		tec_file.precision(12);
		if (!tec_file.good()) return;
#ifdef SUPERCOMPUTER
		// kg44 buffer the output
		char mybuf1[MY_IO_BUFSIZE * MY_IO_BUFSIZE];
		tec_file.rdbuf()->pubsetbuf(mybuf1, MY_IO_BUFSIZE * MY_IO_BUFSIZE);
#endif
		//
		WriteTECHeader(tec_file, te, eleType);
		WriteTECNodePCONData(tec_file);
		WriteTECElementData(tec_file, te);
		tec_file.close();
	}
}

/**************************************************************************
   FEMLib-Method:
   Task: Node value output of PCON in aquous
   Programing:
   08/2008 MX Implementation
**************************************************************************/
void COutput::WriteTECNodePCONData(fstream& tec_file)
{
	const size_t nName(_pcon_value_vector.size());
	int nidx_dm[3];
	std::vector<int> PconIndex(nName);

//  m_msh = GetMSH();

#ifdef CHEMAPP
	CEqlink* eq = NULL;

	eq = eq->GetREACTION();
	if (!eq) return;
	const int nPCON_aq = eq->NPCON[1];  // GetNPCON(1);
	eq->GetPconNameAq();

	for (i = 0; i < nName; i++)
	{
		for (k = 0; k < nPCON_aq; k++)
			//		 pcon_value_name = PconName_Aq[i];
			if (pcon_value_vector[i].compare(PconName_Aq[k]) == 0)
			{
				PconIndex[i] = k;
				break;
			}
	}
#endif
	// MSH
	//--------------------------------------------------------------------
	for (size_t j = 0l; j < m_msh->GetNodesNumber(false); j++)
	{
		// XYZ
		double x[3] = {m_msh->nod_vector[j]->getData()[0],
		               m_msh->nod_vector[j]->getData()[1],
		               m_msh->nod_vector[j]->getData()[2]};
		//      x[0] = m_msh->nod_vector[j]->X();
		//      x[1] = m_msh->nod_vector[j]->Y();
		//      x[2] = m_msh->nod_vector[j]->Z();
		// Amplifying DISPLACEMENTs
		if (M_Process || MH_Process)  // WW

			for (size_t k = 0; k < max_dim + 1; k++)
				x[k] += out_amplifier *
				        m_pcs->GetNodeValue(m_msh->nod_vector[j]->GetIndex(),
				                            nidx_dm[k]);
		for (size_t i = 0; i < 3; i++)
			tec_file << x[i] << " ";
// NOD values
#ifdef CHEMAPP
		for (size_t k = 0; k < nName; k++)
			tec_file << eq->GetPconAq_mol_amount(j, PconIndex[k]) << " ";

#endif
		tec_file << "\n";
	}
}

void COutput::WritePetrelElementData(int time_step_number)
{
	size_t no_ele_values = _ele_value_vector.size();
	if (no_ele_values == 0) return;
	std::cout << "->write results into Peterel file."
	          << "\n";

	if (m_pcs == NULL)
	{
		m_pcs = PCSGet(_ele_value_vector[0], true);
		if (m_pcs == 0) m_pcs = pcs_vector[0];
	}
	const size_t n_e = m_msh->ele_vector.size();

	std::vector<std::string> vec_ele_val_list;
	std::vector<std::vector<double> > vec_ele_vals;
	std::vector<double> temp_nodal_val;
	for (size_t j = 0; j < no_ele_values; j++)
	{
		int nod_var_id = m_pcs->GetNodeValueIndex(_ele_value_vector[j]);
		if (nod_var_id < 0) continue;

		vec_ele_val_list.push_back(_ele_value_vector[j]);
		vec_ele_vals.resize(vec_ele_val_list.size());
		std::vector<double>& vec_ele_val = vec_ele_vals.back();
		vec_ele_val.resize(n_e);
		for (size_t i_e = 0; i_e < n_e; i_e++)
		{
			MeshLib::CElem* e = m_msh->ele_vector[i_e];
			// get nodal values
			temp_nodal_val.resize(e->GetNodesNumber(false));
			for (size_t k = 0; k < temp_nodal_val.size(); k++)
			{
				temp_nodal_val[k] =
				    m_pcs->GetNodeValue(e->GetNodeIndex(k), nod_var_id);
			}
			// average
			double ele_val = .0;
			for (size_t k = 0; k < temp_nodal_val.size(); k++)
			{
				ele_val += temp_nodal_val[k];
			}
			ele_val /= (double)temp_nodal_val.size();
			// set
			vec_ele_val[i_e] = ele_val;
		}
	}

	stringstream stm;
	stm << time_step_number;

	for (size_t j = 0; j < vec_ele_val_list.size(); j++)
	{
		std::string tec_file_name =
		    file_base_name + "_" + vec_ele_val_list[j] + "_";
		tec_file_name += stm.str() + ".pet";

		fstream dat_file(tec_file_name.c_str(), std::ios::out);
		dat_file.setf(std::ios::scientific, std::ios::floatfield);
		dat_file.precision(12);
		if (!dat_file.good()) continue;

		for (size_t i = 0; i < n_e; i++)
		{
			dat_file << i << " ";
			for (size_t j = 0; j < vec_ele_val_list.size(); j++)
				dat_file << vec_ele_vals[j][i] << " ";
			dat_file << "\n";
		}

		dat_file.close();
	}
}

void COutput::checkConsistency()
{
	if (!_nod_value_vector.empty())
	{
		std::vector<std::string> del_index, alias_del_lindex;
		bool found = false;
		CRFProcess* pcs = NULL;
		const size_t pcs_vector_size(pcs_vector.size());
		const size_t nod_value_vector_size(_nod_value_vector.size());
		for (size_t j = 0; j < nod_value_vector_size; j++)
		{
			found = false;  // initialize variable found
			for (size_t l = 0; l < pcs_vector_size; l++)
			{
				pcs = pcs_vector[l];
				for (size_t m = 0; m < pcs->nod_val_name_vector.size(); m++)
					if (pcs->nod_val_name_vector[m].compare(
					        _nod_value_vector[j]) == 0)
					{
						found = true;
						del_index.push_back(_nod_value_vector[j]);
						alias_del_lindex.push_back(_alias_nod_value_vector[j]);
						break;
					}
				// end for(m...)
			}  // end for(l...)
			if (!found)
			{
				std::cout << "Warning - no PCS data for output variable "
				          << _nod_value_vector[j] << " in ";
				switch (getGeoType())
				{
					case GEOLIB::POINT:
						std::cout << "POINT " << getGeoName() << "\n";
						break;
					case GEOLIB::POLYLINE:
						std::cout << "POLYLINE " << getGeoName() << "\n";
						break;
					case GEOLIB::SURFACE:
						std::cout << "SURFACE " << getGeoName() << "\n";
						break;
					case GEOLIB::VOLUME:
						std::cout << "VOLUME " << getGeoName() << "\n";
						break;
					case GEOLIB::GEODOMAIN:
						std::cout << "DOMAIN " << getGeoName() << "\n";
						break;
					case GEOLIB::INVALID:
						std::cout << "WARNING: COutput::checkConsistency - "
						             "invalid geo type" << endl;
						break;
				}
			}
		}  // end for(j...)

		// Reduce vector out->_nod_value_vector by elements which have no PCS
		if (del_index.size() < _nod_value_vector.size())
		{
			std::cout << " Reducing output to variables with existing PCS-data "
			          << "\n";
			_nod_value_vector.clear();
			for (size_t j = 0; j < del_index.size(); j++)
				_nod_value_vector.push_back(del_index[j]);
			_alias_nod_value_vector.clear();
			for (size_t j = 0; j < del_index.size(); j++)
				_alias_nod_value_vector.push_back(alias_del_lindex[j]);
		}
		if (!pcs) pcs = this->GetPCS();
#ifndef NDEBUG
		if (!pcs) cout << "Warning in OUTData - no PCS data" << endl;
#endif
	}  // end if(_nod_value_vector.size()>0)
}

/**************************************************************************
   FEMLib-Method:
   Task: Set output variable names for internal use
   Programing:
   11/2011 NW Implementation
**************************************************************************/
void COutput::setInternalVarialbeNames(CFEMesh* msh)
{
#if 0
    if (_alias_nod_value_vector.empty())
        return;
    bool isXZplane = (msh->GetCoordinateFlag()==22);
    bool isPVD = (dat_type_name.compare("PVD") == 0); //currently only for PVD

    if (isXZplane && isPVD) {
        std::cout << "-> recognized XZ plane for PVD output." << "\n";
        map<string,string> map_output_variable_name;
        map_output_variable_name.insert(pair<string, string>("DISPLACEMENT_Y1", "DISPLACEMENT_Z1" ));
        map_output_variable_name.insert(pair<string, string>("DISPLACEMENT_Z1", "DISPLACEMENT_Y1" ));
        map_output_variable_name.insert(pair<string, string>("STRESS_XY", "STRESS_XZ" ));
        map_output_variable_name.insert(pair<string, string>("STRESS_YY", "STRESS_ZZ" ));
        map_output_variable_name.insert(pair<string, string>("STRESS_ZZ", "STRESS_YY" ));
        map_output_variable_name.insert(pair<string, string>("STRESS_XZ", "STRESS_XY" ));
        map_output_variable_name.insert(pair<string, string>("STRAIN_XY", "STRAIN_XZ" ));
        map_output_variable_name.insert(pair<string, string>("STRAIN_YY", "STRAIN_ZZ" ));
        map_output_variable_name.insert(pair<string, string>("STRAIN_ZZ", "STRAIN_YY" ));
        map_output_variable_name.insert(pair<string, string>("STRAIN_XZ", "STRAIN_XY"  ));
        map_output_variable_name.insert(pair<string, string>("VELOCITY_Y1", "VELOCITY_Z1"));
        map_output_variable_name.insert(pair<string, string>("VELOCITY_Z1", "VELOCITY_Y1"));
        map_output_variable_name.insert(pair<string, string>("VELOCITY_Y2", "VELOCITY_Z2"));
        map_output_variable_name.insert(pair<string, string>("VELOCITY_Z2", "VELOCITY_Y2"));

        for (size_t j = 0; j < _alias_nod_value_vector.size(); j++) {
            if (map_output_variable_name.count(_alias_nod_value_vector[j])>0) {
                _nod_value_vector.push_back(map_output_variable_name[_alias_nod_value_vector[j]]);
            } else {
                _nod_value_vector.push_back(_alias_nod_value_vector[j]);
            }
        }
    } else {
        for (size_t j = 0; j < _alias_nod_value_vector.size(); j++) {
            _nod_value_vector.push_back(_alias_nod_value_vector[j]);
        }
    }
#else
	if (_nod_value_vector.empty()) return;
	bool isXZplane = (msh->GetCoordinateFlag() == 22);
	bool isPVD = (dat_type_name.compare("PVD") == 0);  // currently only for PVD

	if (isXZplane && isPVD)
	{
		std::cout << "-> recognized XZ plane for PVD output."
		          << "\n";
		map<string, string> map_output_variable_name;
		map_output_variable_name.insert(
		    pair<string, string>("DISPLACEMENT_Y1", "DISPLACEMENT_Z1"));
		map_output_variable_name.insert(
		    pair<string, string>("DISPLACEMENT_Z1", "DISPLACEMENT_Y1"));
		map_output_variable_name.insert(
		    pair<string, string>("STRESS_XY", "STRESS_XZ"));
		map_output_variable_name.insert(
		    pair<string, string>("STRESS_YY", "STRESS_ZZ"));
		map_output_variable_name.insert(
		    pair<string, string>("STRESS_ZZ", "STRESS_YY"));
		map_output_variable_name.insert(
		    pair<string, string>("STRESS_XZ", "STRESS_XY"));
		map_output_variable_name.insert(
		    pair<string, string>("STRAIN_XY", "STRAIN_XZ"));
		map_output_variable_name.insert(
		    pair<string, string>("STRAIN_YY", "STRAIN_ZZ"));
		map_output_variable_name.insert(
		    pair<string, string>("STRAIN_ZZ", "STRAIN_YY"));
		map_output_variable_name.insert(
		    pair<string, string>("STRAIN_XZ", "STRAIN_XY"));
		map_output_variable_name.insert(
		    pair<string, string>("VELOCITY_Y1", "VELOCITY_Z1"));
		map_output_variable_name.insert(
		    pair<string, string>("VELOCITY_Z1", "VELOCITY_Y1"));
		map_output_variable_name.insert(
		    pair<string, string>("VELOCITY_Y2", "VELOCITY_Z2"));
		map_output_variable_name.insert(
		    pair<string, string>("VELOCITY_Z2", "VELOCITY_Y2"));

		for (size_t j = 0; j < _nod_value_vector.size(); j++)
		{
			if (map_output_variable_name.count(_nod_value_vector[j]) > 0)
			{
				_alias_nod_value_vector.push_back(
				    map_output_variable_name[_nod_value_vector[j]]);
			}
			else
			{
				_alias_nod_value_vector.push_back(_nod_value_vector[j]);
			}
		}
	}
	else
	{
		for (size_t j = 0; j < _nod_value_vector.size(); j++)
		{
			_alias_nod_value_vector.push_back(_nod_value_vector[j]);
		}
	}
#endif
}

void COutput::addInfoToFileName(std::string& file_name, bool geo, bool process,
                                bool mesh) const
{
	// add geo type name
	if (geo)
		//		file_name += getGeoTypeAsString();
		file_name += geo_name;

	// add process type name
	if (getProcessType() != FiniteElement::INVALID_PROCESS && process)
		file_name +=
		    "_" + FiniteElement::convertProcessTypeToString(getProcessType());

	// add mesh type name
	if (msh_type_name.size() > 0 && mesh) file_name += "_" + msh_type_name;

	// finally add file extension
	if (dat_type_name.compare("CSV") == 0)
		file_name += ".csv";
	else
		file_name += TEC_FILE_EXTENSION;
}

/**************************************************************************
 FEMLib-Method:
 12/2011 JOD Calculates absolute value of volume flux at each note on an upright
 surface (normalvec) for water balance output
 from CSourceTerm::FaceIntegration

 **************************************************************************/
void COutput::CalculateThroughflow(CFEMesh* msh, vector<long>& nodes_on_sfc,
                                   vector<double>& node_value_vector)
{
	CRFProcess* m_pcs = PCSGet(getProcessType());

	if (!msh || !m_pcs)
	{
		std::cout << "Warning in COutput::FaceIntegration: no MSH and / or PCS "
		             " data for water balance";
		return;
	}

	long i, j, k, l, count;
	int nfaces, nfn;
	int nodesFace[8];
	double fac = 1.0, nodesFVal[8], v1[3], v2[3], normal_vector[3],
	       normal_velocity;  //, poro;
	                         //	CMediumProperties *MediaProp;

	int Axisymm = 1;                         // ani-axisymmetry
	if (msh->isAxisymmetry()) Axisymm = -1;  // Axisymmetry is true

	CElem* elem = NULL;
	CElem* face = new CElem(1);
	FiniteElement::CElement* fem =
	    new FiniteElement::CElement(Axisymm * msh->GetCoordinateFlag());
	CNode* e_node = NULL;
	CElem* e_nei = NULL;

	set<long> set_nodes_on_sfc;
	vector<long> vec_possible_elements;
	vector<long> G2L((long)msh->nod_vector.size());
	vector<double> NVal((long)nodes_on_sfc.size());

	// ----- initialize
	// --------------------------------------------------------------------

	for (i = 0; i < (long)msh->nod_vector.size(); i++)
	{
		msh->nod_vector[i]->SetMark(false);
		G2L[i] = -1;
	}

	for (i = 0; i < (long)nodes_on_sfc.size(); i++)
	{
		NVal[i] = 0.0;
		k = nodes_on_sfc[i];
		G2L[k] = i;
	}

	for (i = 0; i < (long)msh->ele_vector.size(); i++)
	{
		msh->ele_vector[i]->selected = 0;  // TODO can use a new variable
	}
	for (i = 0; i < (long)nodes_on_sfc.size(); i++)
	{
		set_nodes_on_sfc.insert(nodes_on_sfc[i]);
	}

	// ---- search elements
	// ------------------------------------------------------------------

	face->SetFace();
	for (i = 0; i < (long)nodes_on_sfc.size(); i++)
	{
		k = nodes_on_sfc[i];
		for (j = 0;
		     j < (long)msh->nod_vector[k]->getConnectedElementIDs().size();
		     j++)
		{
			l = msh->nod_vector[k]->getConnectedElementIDs()[j];
			if (msh->ele_vector[l]->selected == 0)
				vec_possible_elements.push_back(l);
			msh->ele_vector[l]->selected +=
			    1;  // number of elements on the surface
		}
	}

	// ---- face integration
	// ------------------------------------------------------------------

	for (i = 0; i < (long)vec_possible_elements.size(); i++)
	{
		elem = msh->ele_vector[vec_possible_elements[i]];
		//		MediaProp = mmp_vector[elem->GetPatchIndex()];
		// poro = MediaProp->Porosity(elem->GetIndex(), 1.0);

		if (!elem->GetMark()) continue;
		nfaces = elem->GetFacesNumber();
		elem->SetOrder(msh->getOrder());

		for (j = 0; j < nfaces; j++)
		{
			e_nei = elem->GetNeighbor(j);
			nfn = elem->GetElementFaceNodes(j, nodesFace);
			// 1st check
			if (elem->selected < nfn) continue;
			// 2nd check: if all nodes of the face are on the surface
			count = 0;
			for (k = 0; k < nfn; k++)
			{
				e_node = elem->GetNode(nodesFace[k]);
				nodesFVal[k] = 1;
				if (set_nodes_on_sfc.count(e_node->GetIndex()) > 0)
				{
					count++;
				}
			}  // end k
			if (count != nfn) continue;

			// --------- calculate area ----------------------------------

			fac = 1.0;
			if (elem->GetDimension() ==
			    e_nei->GetDimension())  // Not a surface face
				fac = 0.5;

			face->SetFace(elem, j);
			face->SetOrder(msh->getOrder());
			face->ComputeVolume();
			fem->setOrder(msh->getOrder() + 1);
			fem->ConfigElement(face, true);
			fem->FaceIntegration(nodesFVal);

			// --------- construct normal vector
			// ----------------------------------
			MeshLib::CNode const& node0(
			    *(m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]));
			MeshLib::CNode const& node1(
			    *(m_msh->nod_vector[elem->GetNode(nodesFace[1])->GetIndex()]));
			MeshLib::CNode const& node2(
			    *(m_msh->nod_vector[elem->GetNode(nodesFace[2])->GetIndex()]));

			v1[0] = node1[0] - node0[0];
			v1[1] = node1[1] - node0[1];
			v1[2] = node1[2] - node0[2];
			v2[0] = node2[0] - node0[0];
			v2[1] = node2[1] - node0[1];
			v2[2] = node2[2] - node0[2];

			MathLib::crossProd(v1, v2, normal_vector);
			//			CrossProduction(v1, v2, normal_vector);

			if (fabs(normal_vector[0]) >
			    1.e-20)  // standardize direction for ± sign in result
			{
				if (normal_vector[0] < 0)
				{  // point at x-direction
					normal_vector[0] = -normal_vector[0];
					normal_vector[1] = -normal_vector[1];
				}
			}
			else
			{
				if (normal_vector[1] < 0)
				{  // point at y-direction
					normal_vector[0] = -normal_vector[0];
					normal_vector[1] = -normal_vector[1];
				}
			}

			NormalizeVector(normal_vector, 3);

			//			k = 2;
			//			do {
			//				v1[0] =
			// m_msh->nod_vector[elem->GetNode(nodesFace[1])->GetIndex()]->getData()[0]
			//						-
			// m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]->getData()[0];
			//				v1[1] =
			// m_msh->nod_vector[elem->GetNode(nodesFace[1])->GetIndex()]->getData()[1]
			//						-
			// m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]->getData()[1];
			//				v1[2] =
			// m_msh->nod_vector[elem->GetNode(nodesFace[1])->GetIndex()]->getData()[2]
			//						-
			// m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]->getData()[2];
			//				v2[0] =
			// m_msh->nod_vector[elem->GetNode(nodesFace[k])->GetIndex()]->getData()[0]
			//						-
			// m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]->getData()[0];
			//				v2[1] =
			// m_msh->nod_vector[elem->GetNode(nodesFace[k])->GetIndex()]->getData()[1]
			//						-
			// m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]->getData()[1];
			//				v2[2] =
			// m_msh->nod_vector[elem->GetNode(nodesFace[k])->GetIndex()]->getData()[2]
			//						-
			// m_msh->nod_vector[elem->GetNode(nodesFace[0])->GetIndex()]->getData()[2];
			//				k++;
			//			} while (fabs(v1[0] - v2[0]) < 1.e-10 && fabs(v1[1] -
			//v2[1])
			//< 1.e-10 && fabs(v1[2]
			//							- v2[2]) < 1.e-10); // 3 points on surface are
			//not
			// on 1 line
			//
			//
			//			CrossProduction(v1, v2, normal_vector);
			//
			//			/* if(fabs(normal_vector[1])> 1.e-10)
			//			 {
			//			 if(normal_vector[1] < 0)
			//			 {
			//			 normal_vector[0]=-normal_vector[0];
			//			 normal_vector[1]=-normal_vector[1];
			//			 normal_vector[2]=-normal_vector[2];
			//			 }
			//			 }
			//			 else {
			//			 if(normal_vector[0] < 0)
			//			 {
			//			 normal_vector[0]=-normal_vector[0];
			//			 normal_vector[1]=-normal_vector[1];
			//			 normal_vector[2]=-normal_vector[2];
			//			 }
			//			 }
			//			 */
			//
			//			NormalizeVector(normal_vector, 3);

			// ----------- calculate volume flux -------------------------------

			for (k = 0; k < nfn; k++)
			{
				e_node = elem->GetNode(nodesFace[k]);

				v1[0] = m_pcs->GetNodeValue(e_node->GetIndex(),
				                            7);  // get velocity (flux)
				v1[1] = m_pcs->GetNodeValue(e_node->GetIndex(), 8);
				v1[2] = m_pcs->GetNodeValue(e_node->GetIndex(), 9);

				normal_velocity = PointProduction(v1, normal_vector);

				// NVal[G2L[e_node->GetIndex()]] += (fac*nodesFVal[k] *
				// normal_velocity );      // JOD poro???
				NVal[G2L[e_node->GetIndex()]] +=
				    fabs(fac * nodesFVal[k] * normal_velocity);  // JOD poro???
			}
		}  // end faces
	}      // end possible elements

	for (i = 0; i < (long)nodes_on_sfc.size(); i++)
		node_value_vector[i] = NVal[i];  // result:  | volume flux |_1

	NVal.clear();
	G2L.clear();
	delete fem;
	delete face;
}

/**************************************************************************
 FEMLib-Method:
 Task:   Write water balance
 Use:    specify  $DAT_TYPE as WATER_BALANCE
 Programing:
 06/2012 JOD Implementation
 **************************************************************************/
void COutput::NODWriteWaterBalance(double time_current)
{
	switch (getGeoType())
	{
		case GEOLIB::SURFACE:
			NODWriteWaterBalanceSFC(time_current);
			break;
		case GEOLIB::POLYLINE:
			NODWriteWaterBalancePLY(time_current);
			break;
		case GEOLIB::POINT:
			NODWriteWaterBalancePNT(time_current);
			break;
		default:
			break;
	}
}
/**************************************************************************
 FEMLib-Method:
 Task:   Write water balance for polyline with leakance
 Use:
 Programing:
 06/2012 JOD Implementation
 **************************************************************************/

void COutput::NODWriteWaterBalancePNT(double time_current)
{
	CFEMesh* m_msh = NULL;
	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));
	CRFProcess* m_pcs = NULL;
	m_pcs = PCSGet(getProcessType());
	long msh_node_number(
	    m_msh->GetNODOnPNT(static_cast<const GEOLIB::Point*>(getGeoObj())));
	double volume_flux;
	//--------------------------------------------------------------------
	// File handling
	char number_char[3];
	string number_string = number_char;
	string tec_file_name = convertProcessTypeToString(getProcessType()) +
	                       "_pnt_" + geo_name + "_water_balance.txt";
	if (_time < 1.e-20)  // simulation must start at t= 0!!!
	{
		remove(tec_file_name.c_str());
		return;
	}
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
	//--------------------------------------------------------------------
	// los geht's

	volume_flux = m_pcs->GetNodeValue(msh_node_number,
	                                  m_pcs->GetNodeValueIndex("FLUX") + 1) *
	              (m_pcs->GetNodeValue(msh_node_number,
	                                   m_pcs->GetNodeValueIndex("HEAD") + 1) -
	               m_pcs->m_msh->nod_vector[msh_node_number]->getData()[2]);

	tec_file << "time: " << time_current
	         << " groundwater discharge (m^3/s): " << volume_flux << "\n";

	tec_file.close();
}

/**************************************************************************
 FEMLib-Method:
 Task:   Write water balance for polyline with leakance
 Use:
 Programing:
 06/2012 JOD Implementation
 **************************************************************************/

void COutput::NODWriteWaterBalancePLY(double /*time_current*/)
{
	CFEMesh* m_msh = NULL;
	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));
	CRFProcess* m_pcs = NULL;
	m_pcs = PCSGet(getProcessType());
	vector<long> nodes_vector;
	vector<double> nodes_vector_volume_flux;
	GEOLIB::Polyline const* const ply(
	    dynamic_cast<GEOLIB::Polyline const* const>(this->getGeoObj()));
	double volume_flux, volume_flux_node;

	//--------------------------------------------------------------------
	// File handling
	char number_char[3];
	string number_string = number_char;
	string tec_file_name = convertProcessTypeToString(getProcessType()) +
	                       "_ply_" + geo_name + "_water_balance.txt";
	if (_time < 1.e-20)  // simulation must start at t= 0!!!
	{
		remove(tec_file_name.c_str());
		return;
	}
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
	//--------------------------------------------------------------------
	// los geht's

	if (ply)
	{
		m_msh->GetNODOnPLY(ply, nodes_vector);
		volume_flux = 0;
		tec_file << "time: " << _time << "\n";
		for (int i = 0; i < (long)nodes_vector.size(); i++)
		{
			volume_flux_node =
			    m_pcs->GetNodeValue(nodes_vector[i],
			                        m_pcs->GetNodeValueIndex("FLUX") + 1) *
			    (m_pcs->GetNodeValue(nodes_vector[i],
			                         m_pcs->GetNodeValueIndex("HEAD") + 1) -
			     m_pcs->m_msh->nod_vector[nodes_vector[i]]->getData()[2]);
			tec_file << " mesh node: " << nodes_vector[i] << " x: "
			         << m_msh->nod_vector[nodes_vector[i]]->getData()[0]
			         << " y: "
			         << m_msh->nod_vector[nodes_vector[i]]->getData()[1]
			         << " z: "
			         << m_msh->nod_vector[nodes_vector[i]]->getData()[2]
			         << " groundwater discharge (m^3/s): " << volume_flux_node
			         << "\n";
			volume_flux += volume_flux_node;
		}
		tec_file << " total groundwater discharge (m^3/s): " << volume_flux
		         << "\n";
	}  // m_ply

	tec_file.close();
}

/**************************************************************************
 FEMLib-Method:
 Task:   Write water balance for up-right surface
 Use:
 Programing:
 06/2012 JOD Implementation
 **************************************************************************/

void COutput::NODWriteWaterBalanceSFC(double time_current)
{
	CFEMesh* m_msh(MeshLib::FEMGet(convertProcessTypeToString(getProcessType())));
	//	CRFProcess* m_pcs (PCSGet(getProcessType()));
	Surface* m_sfc = NULL;
	m_sfc = GEOGetSFCByName(geo_name);

	vector<long> nodes_vector;
	vector<double> nodes_vector_volume_flux;

	double volume_flux;
	long i;

	//--------------------------------------------------------------------
	// File handling
	char number_char[3];
	string number_string = number_char;
	string tec_file_name = convertProcessTypeToString(getProcessType()) +
	                       "_sfc_" + geo_name + "_water_balance.txt";
	if (_time < 1.e-20)  // simulation must start at t= 0!!!
	{
		remove(tec_file_name.c_str());
		return;
	}
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);
	//--------------------------------------------------------------------
	// los geht's

	if (m_sfc)
	{
		m_msh->GetNODOnSFC(m_sfc, nodes_vector);
		nodes_vector_volume_flux.resize(nodes_vector.size());
		CalculateThroughflow(m_msh, nodes_vector, nodes_vector_volume_flux);

		volume_flux = 0;
		for (i = 0; i < (long)nodes_vector_volume_flux.size(); i++)
		{
			volume_flux += nodes_vector_volume_flux[i];
			cout << nodes_vector_volume_flux[i] << " ";
		}
		tec_file << time_current << " " << volume_flux << "\n";

	}  // m_sfc

	tec_file.close();
}

/**************************************************************************
 FEMLib-Method:
 Task:   Write output of multiple points in single file
 Use:    Specify  $DAT_TYPE as COMBINE_POINTS
 Programing:
 06/2012 JOD Implementation
 **************************************************************************/
void COutput::NODWritePointsCombined(double time_current)
{
	CFEMesh* m_msh = NULL;
	m_msh = MeshLib::FEMGet(convertProcessTypeToString(getProcessType()));
	CRFProcess* m_pcs_out = NULL;
	m_pcs_out = PCSGet(getProcessType());

	// std::string tec_file_name(file_base_name + "_time_");
	// addInfoToFileName(tec_file_name, true, true, true);

	char number_char[3];
	string number_string = number_char;
	string tec_file_name =
	    convertProcessTypeToString(getProcessType()) + "_time_" + "POINTS";
	if (_time < 1.e-20)
	{
		remove(tec_file_name.c_str());
		return;
	}
	fstream tec_file(tec_file_name.data(), ios::app | ios::out);
	tec_file.setf(ios::scientific, ios::floatfield);
	tec_file.precision(12);
	if (!tec_file.good()) return;
	tec_file.seekg(0L, ios::beg);

	long msh_node_number(
	    m_msh->GetNODOnPNT(static_cast<const GEOLIB::Point*>(getGeoObj())));

	//----------------------------------------------------------------------
	// NIDX for output variables
	size_t no_variables(_nod_value_vector.size());
	vector<int> NodeIndex(no_variables);
	GetNodeIndexVector(NodeIndex);

	//   int no_variables = (int)nod_value_vector.size();
	// vector<int>NodeIndex(no_variables);

	tec_file << geo_name << " ";
	std::string nod_value_name;
	// int nidx = m_pcs_out->GetNodeValueIndex(nod_value_name) + 1;

	double val_n;

	for (size_t i = 0; i < _nod_value_vector.size(); i++)
	{
		nod_value_name = _nod_value_vector[i];
		val_n = m_pcs_out->GetNodeValue(msh_node_number, NodeIndex[i]);
		tec_file << "time " << time_current << " " << nod_value_name << " "
		         << val_n << " "
		         << "\n";
	}

	tec_file.close();
}
