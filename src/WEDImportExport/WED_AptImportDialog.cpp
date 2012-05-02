/* 
 * Copyright (c) 2012, Laminar Research.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 *
 */

#include "WED_AptImportDialog.h"
#include "WED_AptIE.h"
#include "WED_ToolUtils.h"
#include "WED_Thing.h"
#include "WED_Colors.h"
#include "GUI_Resources.h"
#include "GUI_Packer.h"
#include "GUI_Button.h"
#include "WED_FilterBar.h"
#include "GUI_Messages.h"
#include "STLUtils.h"
#include "WED_Messages.h"
#include "WED_Document.h"

static int import_bounds_default[4] = { 0, 0, 500, 500 };
static int kDefCols[] = { 100, 100 };

enum {
	kMsg_FilterChanged = WED_PRIVATE_MSG_BASE,
	kMsgImport,
	kMsgCancel
};

WED_AptImportDialog::WED_AptImportDialog(
		GUI_Commander * cmdr, 
		AptVector&		apts,
		const char *	file_path,
		WED_Document *	resolver, 
		WED_Archive *	archive) : 
	GUI_Window("Import apt.dat", xwin_style_resizable|xwin_style_visible|xwin_style_centered, import_bounds_default, cmdr),
	GUI_SimpleTableGeometry(2,kDefCols,20),
	mTextTable(this,100,0),
	mResolver(resolver),
	mArchive(archive),
	mPath(file_path),
	mSortColumn(1),
	mInvertSort(1)	
{
	resolver->AddListener(this);
	
	swap(mApts,apts);

	int bounds[4];
	GUI_Packer * packer = new GUI_Packer;
	packer->SetParent(this);
	packer->SetSticky(1,1,1,1);
	packer->Show();
	GUI_Pane::GetBounds(bounds);
	packer->SetBounds(bounds);
	packer->SetBkgkndImage ("gradient.png");
	

	mFilter = new WED_FilterBar(this,kMsg_FilterChanged,0,"Filter:","");
	mFilter->Show();
	mFilter->SetSticky(1,0,1,1);
	mFilter->SetParent(packer);
	mFilter->AddListener(this);

	resort();
	
	mScroller = new GUI_ScrollerPane(0,1);
	mScroller->SetParent(this);
	mScroller->Show();
	mScroller->SetSticky(1,1,1,1);

	mTextTable.SetProvider(this);
	mTextTable.SetGeometry(this);

	mTextTable.SetColors(
				WED_Color_RGBA(wed_Table_Gridlines),
				WED_Color_RGBA(wed_Table_Select),
				WED_Color_RGBA(wed_Table_Text),
				WED_Color_RGBA(wed_Table_SelectText),
				WED_Color_RGBA(wed_Table_Drag_Insert),
				WED_Color_RGBA(wed_Table_Drag_Into));
	mTextTable.SetTextFieldColors(
				WED_Color_RGBA(wed_TextField_Text),
				WED_Color_RGBA(wed_TextField_Hilite),
				WED_Color_RGBA(wed_TextField_Bkgnd),
				WED_Color_RGBA(wed_TextField_FocusRing));

	mTable = new GUI_Table(true);
	mTable->SetGeometry(this);
	mTable->SetContent(&mTextTable);
	mTable->SetParent(mScroller);
	mTable->SetSticky(1,1,1,1);
	mTable->Show();	
	mScroller->PositionInContentArea(mTable);
	mScroller->SetContent(mTable);
	mTextTable.SetParentTable(mTable);

	mTextTableHeader.SetProvider(this);
	mTextTableHeader.SetGeometry(this);

	mTextTableHeader.SetImage("header.png");
	mTextTableHeader.SetColors(
			WED_Color_RGBA(wed_Table_Gridlines),
				WED_Color_RGBA(wed_Header_Text));

	mHeader = new GUI_Header(true);

	bounds[1] = 0;
	bounds[3] = GUI_GetImageResourceHeight("header.png") / 2;
	mHeader->SetBounds(bounds);
	mHeader->SetGeometry(this);
	mHeader->SetHeader(&mTextTableHeader);
	mHeader->SetParent(this);
	mHeader->Show();
	mHeader->SetSticky(1,0,1,1);
	mHeader->SetTable(mTable);


					mTextTableHeader.AddListener(mHeader);		// Header listens to text table to know when to refresh on col resize
					mTextTableHeader.AddListener(mTable);		// Table listense to text table header to announce scroll changes (and refresh) on col resize
					mTextTable.AddListener(mTable);				// Table listens to text table to know when content changes in a resizing way
					this->AddListener(mTable);			// Table listens to actual property content to know when data itself changes

	packer->PackPane(mFilter,gui_Pack_Top);
	packer->PackPane(mHeader,gui_Pack_Top);
	
	int k_reg[4] = { 0, 0, 1, 3 };
	int k_hil[4] = { 0, 1, 1, 3 };
	
	GUI_Button * okay_btn = new GUI_Button("push_buttons.png",btn_Push,k_reg, k_hil,k_reg,k_hil);
	okay_btn->SetBounds(105,5,205,GUI_GetImageResourceHeight("push_buttons.png") / 3);
	okay_btn->Show();
	okay_btn->SetSticky(0,1,1,0);
	okay_btn->SetDescriptor("Import");
	okay_btn->SetMsg(kMsgImport,0);

	GUI_Button * cncl_btn = new GUI_Button("push_buttons.png",btn_Push,k_reg, k_hil,k_reg,k_hil);
	cncl_btn->SetBounds(5,5,105,GUI_GetImageResourceHeight("push_buttons.png") / 3);
	cncl_btn->Show();
	cncl_btn->SetSticky(1,1,0,0);
	cncl_btn->SetDescriptor("Cancel");
	cncl_btn->SetMsg(kMsgCancel,0);
	
	GUI_Pane * holder = new GUI_Pane;
	holder->SetBounds(0,0,210,GUI_GetImageResourceHeight("push_buttons.png") / 3 + 10);
	
	
	okay_btn->SetParent(holder);
	okay_btn->AddListener(this);
	cncl_btn->SetParent(holder);
	cncl_btn->AddListener(this);
	
	holder->SetParent(packer);
	holder->Show();
	holder->SetSticky(1,1,1,0);
	
	packer->PackPane(holder,gui_Pack_Bottom);

	
	packer->PackPane(mScroller,gui_Pack_Center);

	mScroller->PositionHeaderPane(mHeader);
}

void WED_AptImportDialog::DoIt(void)
{
	WED_Thing * wrl = WED_GetWorld(mResolver);

	AptVector apts;
	for(int n = 0; n < mSorted.size(); ++n)
	if(mSelected.count(mSorted[n]))
	apts.push_back(mApts[mSorted[n]]);
	
	if(!apts.empty())
	{
		wrl->StartOperation("Import apt.dat");
		WED_AptImport(mArchive, wrl, mPath.c_str(), apts);
		WED_SetAnyAirport(mResolver);
		wrl->CommitOperation();
	}
}

WED_AptImportDialog::~WED_AptImportDialog()
{
}

bool	WED_AptImportDialog::Closed(void)
{
	return true;
}

void	WED_AptImportDialog::SelectHeaderCell(
						int							cell_x)
{
	if(cell_x == mSortColumn)
		mInvertSort = !mInvertSort;
	else
	{
		mSortColumn = cell_x;
		mInvertSort = 1;
	}
	resort();
	Refresh();
}

void	WED_AptImportDialog::GetHeaderContent(
						int							cell_x,
						GUI_HeaderContent&			the_content)
{
	the_content.is_selected = (cell_x == mSortColumn);
	the_content.can_resize = 1;
	the_content.can_select = 1;
	switch(cell_x) {
	case 0:
		the_content.title = "ICAO";
		break;
	case 1:
		the_content.title = "Name";
		break;
	}		
}

int		WED_AptImportDialog::GetColCount(void)
{
	return 2;
}

int		WED_AptImportDialog::GetRowCount(void)
{
	return mSorted.size();
}


void	WED_AptImportDialog::GetCellContent(
					int							cell_x,
					int							cell_y,
					GUI_CellContent&			the_content)
{
	the_content.content_type = gui_Cell_EditText;
	the_content.can_edit = 0;
	the_content.can_disclose = 0;
	the_content.can_select = 1;
	the_content.can_drag = 0;

	int apt_id = mSorted[cell_y];

	the_content.is_disclosed = 0;
	the_content.is_selected = mSelected.count(apt_id);
	the_content.indent_level = 0;


	switch(cell_x) {
	case 0:		
		the_content.text_val = mApts[apt_id].icao;
		break;
	case 1:		
		the_content.text_val = mApts[apt_id].name;
		break;
	}
	the_content.string_is_resource = 0;
}

void	WED_AptImportDialog::SelectionStart(
					int							clear)
{
	if(clear)
		mSelected.clear();
	mSelectedOrig = mSelected;
}

int		WED_AptImportDialog::SelectGetExtent(
					int&						low_x,
					int&						low_y,
					int&						high_x,
					int&						high_y)
{
	if(mSorted.empty())	
		return 0;
	low_x = 0;
	high_x = 0;
	low_y = mSorted.size();
	high_y = 0;
	for(int i = 0; i < mSorted.size(); ++i)
	if(mSelected.count(mSorted[i]))
	{
		low_y = min(low_y,i);
		high_y = max(high_y,i);
	}
	if(low_y <= high_y)
		return 1;
	else
		return 0;
}

int		WED_AptImportDialog::SelectGetLimits(
					int&						low_x,
					int&						low_y,
					int&						high_x,
					int&						high_y)
{
	if(mSorted.empty())	return 0;
	low_x = 0;
	low_y = 0;
	high_x = 0;
	high_y = mSorted.size()-1;
	return 1;
}

void	WED_AptImportDialog::SelectRange(
					int							start_x,
					int							start_y,
					int							end_x,
					int							end_y,
					int							is_toggle)
{
	mSelected = mSelectedOrig;

	for(int x = start_y; x <= end_y; ++x)
	{
		int apt_id = mSorted[x];
		if(is_toggle && mSelected.count(apt_id))		mSelected.erase (apt_id);
		else											mSelected.insert(apt_id);		
	}
	BroadcastMessage(GUI_TABLE_CONTENT_CHANGED,0);
}

void	WED_AptImportDialog::SelectionEnd(void)
{
}

int		WED_AptImportDialog::SelectDisclose(
					int							open_it,
					int							all)
{
	return 0;
}

int		WED_AptImportDialog::TabAdvance(
					int&						io_x,
					int&						io_y,
					int							reverse,
					GUI_CellContent&			the_content)
{
	return 0;
}

int		WED_AptImportDialog::DoubleClickCell(
					int							cell_x,
					int							cell_y)
{
	return 0;
}

void	WED_AptImportDialog::ReceiveMessage(
							GUI_Broadcaster *		inSrc,
							intptr_t    			inMsg,
							intptr_t				inParam)
{
	switch(inMsg) {
	case kMsg_FilterChanged:	
		resort();
		break;
	case kMsgImport:
		DoIt();
		this->AsyncDestroy();
		break;
	case msg_DocumentDestroyed:
	case kMsgCancel:
		this->AsyncDestroy();
		break;
	}
}

void toupper(string& io_string)
{
	for(int i = 0; i < io_string.size(); ++i)
		io_string[i] = toupper(io_string[i]);
}

struct sort_by_apt {
	sort_by_apt(const AptVector * apts, int sort_column, int invert_sort) : apts_(apts), sort_column_(sort_column), invert_sort_(invert_sort) { }

	bool operator()(int x, int y) const {
		string xs(sort_column_ ? apts_->at(x).name : apts_->at(x).icao);
		string ys(sort_column_ ? apts_->at(y).name : apts_->at(y).icao);
		toupper(xs);
		toupper(ys);
		if(invert_sort_)
			return ys < xs;
		else
			return xs < ys;
	}

	int sort_column_;
	int invert_sort_;
	const AptVector * apts_;
};

void		WED_AptImportDialog::resort(void)
{
	string	fstr = mFilter->GetText();
	vector<string>	filters;
	tokenize_string_func(fstr.begin(),fstr.end(),back_inserter(filters),::isspace);

	mSorted.clear();
	for(int i = 0; i < mApts.size(); ++i)
	{
		if (filters.empty() ||
			filter_match(mApts[i].icao, filters.begin(),filters.end()) ||
			filter_match(mApts[i].name, filters.begin(),filters.end()))
		{
			mSorted.push_back(i);
		}
	}
	sort(mSorted.begin(),mSorted.end(), sort_by_apt(&mApts, mSortColumn,mInvertSort));
	BroadcastMessage(GUI_TABLE_CONTENT_RESIZED,0);
}
