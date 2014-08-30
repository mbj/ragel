/*
 *  Copyright 2001-2014 Adrian Thurston <thurston@complang.org>
 */

/*  This file is part of Ragel.
 *
 *  Ragel is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Ragel is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Ragel; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ragel.h"
#include "binloop.h"
#include "redfsm.h"
#include "gendata.h"

BinaryLooped::BinaryLooped( const CodeGenArgs &args )
:
	Binary( args )
{}

/* Determine if we should use indicies or not. */
void BinaryLooped::calcIndexSize()
{
//	long long sizeWithInds =
//		indicies.size() +
//		transCondSpacesWi.size() +
//		transOffsetsWi.size() +
//		transLengthsWi.size();

//	long long sizeWithoutInds =
//		transCondSpaces.size() +
//		transOffsets.size() +
//		transLengths.size();

	//std::cerr << "sizes: " << sizeWithInds << " " << sizeWithoutInds << std::endl;

	///* If using indicies reduces the size, use them. */
	//useIndicies = sizeWithInds < sizeWithoutInds;
	useIndicies = false;
}


void BinaryLooped::tableDataPass()
{
	taActions();
	taKeyOffsets();
	taSingleLens();
	taRangeLens();
	taIndexOffsets();
	taIndicies();

	taTransCondSpacesWi();
	taTransOffsetsWi();
	taTransLengthsWi();

	taTransCondSpaces();
	taTransOffsets();
	taTransLengths();

	taCondTargs();
	taCondActions();

	taToStateActions();
	taFromStateActions();
	taEofActions();

	taEofTransDirect();
	taEofTransIndexed();

	taKeys();
	taCondKeys();
}

void BinaryLooped::genAnalysis()
{
	redFsm->sortByStateId();

	/* Choose default transitions and the single transition. */
	redFsm->chooseDefaultSpan();

	/* Choose the singles. */
	redFsm->chooseSingle();

	/* If any errors have occured in the input file then don't write anything. */
	if ( gblErrorCount > 0 )
		return;

	/* Anlayze Machine will find the final action reference counts, among other
	 * things. We will use these in reporting the usage of fsm directives in
	 * action code. */
	analyzeMachine();

	setKeyType();

	/* Run the analysis pass over the table data. */
	setTableState( TableArray::AnalyzePass );
	tableDataPass();

	/* Determine if we should use indicies. */
	calcIndexSize();

	/* Switch the tables over to the code gen mode. */
	setTableState( TableArray::GeneratePass );
}


void BinaryLooped::COND_ACTION( RedCondAp *cond )
{
	int act = 0;
	if ( cond->action != 0 )
		act = cond->action->location+1;
	condActions.value( act );
}

void BinaryLooped::TO_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->toStateAction != 0 )
		act = state->toStateAction->location+1;
	toStateActions.value( act );
}

void BinaryLooped::FROM_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->fromStateAction != 0 )
		act = state->fromStateAction->location+1;
	fromStateActions.value( act );
}

void BinaryLooped::EOF_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->eofAction != 0 )
		act = state->eofAction->location+1;
	eofActions.value( act );
}

std::ostream &BinaryLooped::TO_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numToStateRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t case " << act->actionId << " {\n";
			ACTION( out, act, IlOpts( 0, false, false ) );
			out << "\n\t}\n";
		}
	}

	return out;
}

std::ostream &BinaryLooped::FROM_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numFromStateRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t case " << act->actionId << " {\n";
			ACTION( out, act, IlOpts( 0, false, false ) );
			out << "\n\t}\n";
		}
	}

	return out;
}

std::ostream &BinaryLooped::EOF_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numEofRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t case " << act->actionId << " {\n";
			ACTION( out, act, IlOpts( 0, true, false ) );
			out << "\n\t}\n";
		}
	}

	return out;
}


std::ostream &BinaryLooped::ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numTransRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t case " << act->actionId << " {\n";
			ACTION( out, act, IlOpts( 0, false, false ) );
			out << "\n\t}\n";
		}
	}

	return out;
}


void BinaryLooped::writeData()
{
	/* If there are any transtion functions then output the array. If there
	 * are none, don't bother emitting an empty array that won't be used. */
	if ( redFsm->anyActions() )
		taActions();

	taKeyOffsets();
	taKeys();
	taSingleLens();
	taRangeLens();
	taIndexOffsets();

	if ( useIndicies ) {
		taIndicies();
		taTransCondSpacesWi();
		taTransOffsetsWi();
		taTransLengthsWi();
	}
	else {
		taTransCondSpaces();
		taTransOffsets();
		taTransLengths();
	}

	taCondKeys();

	taCondTargs();
	taCondActions();

	if ( redFsm->anyToStateActions() )
		taToStateActions();

	if ( redFsm->anyFromStateActions() )
		taFromStateActions();

	if ( redFsm->anyEofActions() )
		taEofActions();

	if ( redFsm->anyEofTrans() ) {
		taEofTransIndexed();
		taEofTransDirect();
	}

	STATE_IDS();
}

void BinaryLooped::writeExec()
{
	testEofUsed = false;
	outLabelUsed = false;

	out <<
		"	{\n"
		"	int _klen;\n";

	if ( redFsm->anyRegCurStateRef() )
		out << "	int _ps;\n";

	out <<
		"	uint _trans = 0;\n"
		"	uint _cond = 0;\n";

	if ( redFsm->anyRegNbreak() )
		out << "	int _nbreak;\n";

	if ( redFsm->anyToStateActions() || redFsm->anyRegActions()
			|| redFsm->anyFromStateActions() )
	{
		out <<
			"	index " << ARR_TYPE( actions ) << " _acts;\n"
			"	uint _nacts;\n";
	}

	out <<
		"	index " << ALPH_TYPE() << " _keys;\n"
		"	index " << ARR_TYPE( condKeys ) << " _ckeys;\n"
		"	int _cpc;\n"
		"	entry {\n"
		"\n";

	if ( !noEnd ) {
		testEofUsed = true;
		out <<
			"	if ( " << P() << " == " << PE() << " )\n"
			"		goto _test_eof;\n";
	}

	if ( redFsm->errState != 0 ) {
		outLabelUsed = true;
		out <<
			"	if ( " << vCS() << " == " << redFsm->errState->id << " )\n"
			"		goto _out;\n";
	}

	out << "label _resume {\n";

	if ( redFsm->anyFromStateActions() ) {
		out <<
			"	_acts = offset( " << ARR_REF( actions ) << ", " << ARR_REF( fromStateActions ) <<
					"[" << vCS() << "]" << " );\n"
			"	_nacts = (uint) deref( " << ARR_REF( actions ) << ", _acts );\n"
			"	_acts += 1;\n"
			"	while ( _nacts > 0 ) {\n"
			"		switch ( deref( " << ARR_REF( actions ) << ", _acts ) ) {\n";
			FROM_STATE_ACTION_SWITCH() <<
			"		}\n"
			"		_nacts -= 1;\n"
			"		_acts += 1;\n"
			"	}\n"
			"\n";
	}

	LOCATE_TRANS();

	out << "}\n";
	out << "label _match {\n";

	if ( useIndicies )
		out << "	_trans = " << ARR_REF( indicies ) << "[_trans];\n";

	LOCATE_COND();

	out << "}\n";
	out << "label _match_cond {\n";

	if ( redFsm->anyRegCurStateRef() )
		out << "	_ps = " << vCS() << ";\n";

	out <<
		"	" << vCS() << " = (int)" << ARR_REF( condTargs ) << "[_cond];\n"
		"\n";

	if ( redFsm->anyRegActions() ) {
		out <<
			"	if ( " << ARR_REF( condActions ) << "[_cond] == 0 )\n"
			"		goto _again;\n"
			"\n";

		if ( redFsm->anyRegNbreak() )
			out << "	_nbreak = 0;\n";

		out <<
			"	_acts = offset( " << ARR_REF( actions ) << ", " << ARR_REF( condActions ) << "[_cond]" << " );\n"
			"	_nacts = (uint) deref( " << ARR_REF( actions ) << ", _acts );\n"
			"	_acts += 1;\n"
			"	while ( _nacts > 0 )\n	{\n"
			"		switch ( deref( " << ARR_REF( actions ) << ", _acts ) )\n"
			"		{\n";
			ACTION_SWITCH() <<
			"		}\n"
			"		_nacts -= 1;\n"
			"		_acts += 1;\n"
			"	}\n"
			"\n";

		if ( redFsm->anyRegNbreak() ) {
			out <<
				"	if ( _nbreak == 1 )\n"
				"		goto _out;\n";
			outLabelUsed = true;
		}
		out << "\n";
	}

//	if ( redFsm->anyRegActions() || redFsm->anyActionGotos() ||
//			redFsm->anyActionCalls() || redFsm->anyActionRets() )
	out << "}\n";
	out << "label _again {\n";

	if ( redFsm->anyToStateActions() ) {
		out <<
			"	_acts = offset( " << ARR_REF( actions ) << ", " << ARR_REF( toStateActions ) <<
					"[" << vCS() << "]" << " );\n"
			"	_nacts = (uint) deref( " << ARR_REF( actions ) << ", _acts );\n"
			"	_acts += 1;\n"
			"	while ( _nacts > 0 ) {\n"
			"		switch ( deref( " << ARR_REF( actions ) << ", _acts ) ) {\n";
			TO_STATE_ACTION_SWITCH() <<
			"		}\n"
			"		_nacts -= 1;\n"
			"		_acts += 1;\n"
			"	}\n"
			"\n";
	}

	if ( redFsm->errState != 0 ) {
		outLabelUsed = true;
		out <<
			"	if ( " << vCS() << " == " << redFsm->errState->id << " )\n"
			"		goto _out;\n";
	}

	if ( !noEnd ) {
		out <<
			"	" << P() << " += 1;\n"
			"	if ( " << P() << " != " << PE() << " )\n"
			"		goto _resume;\n";
	}
	else {
		out <<
			"	" << P() << " += 1;\n"
			"	goto _resume;\n";
	}

	if ( testEofUsed )
		out << "}\n label _test_eof { {}\n";

	if ( redFsm->anyEofTrans() || redFsm->anyEofActions() ) {
		out <<
			"	if ( " << P() << " == " << vEOF() << " )\n"
			"	{\n";

		if ( redFsm->anyEofTrans() ) {
			TableArray &eofTrans = useIndicies ? eofTransIndexed : eofTransDirect;
			out <<
				"	if ( " << ARR_REF( eofTrans ) << "[" << vCS() << "] > 0 ) {\n"
				"		_trans = (uint)" << ARR_REF( eofTrans ) << "[" << vCS() << "] - 1;\n"
				"		_cond = (uint)" << ARR_REF( transOffsets ) << "[_trans];\n"
				"		goto _match_cond;\n"
				"	}\n";
		}

		if ( redFsm->anyEofActions() ) {
			out <<
				"	index " << ARR_TYPE( actions ) << " __acts;\n"
				"	uint __nacts;\n"
				"	__acts = offset( " << ARR_REF( actions ) << ", " << ARR_REF( eofActions ) << "[" << vCS() << "]" << " );\n"
				"	__nacts = (uint) deref( " << ARR_REF( actions ) << ", __acts );\n"
				"	__acts += 1;\n"
				"	while ( __nacts > 0 ) {\n"
				"		switch ( deref( " << ARR_REF( actions ) << ", __acts ) ) {\n";
				EOF_ACTION_SWITCH() <<
				"		}\n"
				"		__nacts -= 1;\n"
				"		__acts += 1;\n"
				"	}\n";
		}

		out <<
			"	}\n"
			"\n";
	}

	if ( outLabelUsed )
		out << "} label _out { {}\n";

	/* The entry loop. */
	out << "}}\n";

	/* The execute block. */
	out << "	}\n";
}
