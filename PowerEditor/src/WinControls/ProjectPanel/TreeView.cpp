//this file is part of notepad++
//Copyright (C)2011 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "precompiledHeaders.h"
#include "TreeView.h"

#define CY_ITEMHEIGHT     18

void TreeView::init(HINSTANCE hInst, HWND parent, int treeViewID)
{
	Window::init(hInst, parent);
	_hSelf = ::GetDlgItem(parent, treeViewID);

	_hSelf = CreateWindowEx(0,
                            WC_TREEVIEW,
                            TEXT("Tree View"),
                            TVS_HASBUTTONS | WS_CHILD | WS_BORDER | WS_HSCROLL |
							TVS_HASLINES | TVS_HASBUTTONS | TVS_SHOWSELALWAYS | TVS_EDITLABELS |  TVS_INFOTIP | WS_TABSTOP, 
                            0,  0,  0, 0,
                            _hParent, 
                            NULL, 
                            _hInst, 
                            (LPVOID)0);

	TreeView_SetItemHeight(_hSelf, CY_ITEMHEIGHT);

	::SetWindowLongPtr(_hSelf, GWLP_USERDATA, (LONG_PTR)this);
	_defaultProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(_hSelf, GWLP_WNDPROC, (LONG_PTR)staticProc));
}


void TreeView::destroy()
{
	HTREEITEM root = TreeView_GetRoot(_hSelf);
	cleanSubEntries(root);
	::DestroyWindow(_hSelf);
	_hSelf = NULL;
} 

LRESULT TreeView::runProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch(Message)
	{
		
		case WM_KEYDOWN:
			if (wParam == VK_F2)
				::MessageBoxA(NULL, "VK_F2", "", MB_OK);
			break;
		default:
			return ::CallWindowProc(_defaultProc, hwnd, Message, wParam, lParam);
	}
	
	return ::CallWindowProc(_defaultProc, hwnd, Message, wParam, lParam);
}

HTREEITEM TreeView::addItem(const TCHAR *itemName, HTREEITEM hParentItem, int iImage, const TCHAR *filePath)
{
	TVITEM tvi;
	tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM; 

	// Set the item label.
	tvi.pszText = (LPTSTR)itemName; 
	tvi.cchTextMax = sizeof(tvi.pszText)/sizeof(tvi.pszText[0]); 

	// Set icon
	tvi.iImage = iImage;//isNode?INDEX_CLOSED_NODE:INDEX_LEAF; 
	tvi.iSelectedImage = iImage;//isNode?INDEX_OPEN_NODE:INDEX_LEAF; 

	// Save the full path of file in the item's application-defined data area. 
	tvi.lParam = (filePath == NULL?0:(LPARAM)(new generic_string(filePath)));

	TVINSERTSTRUCT tvInsertStruct;
	tvInsertStruct.item = tvi; 
	tvInsertStruct.hInsertAfter = (HTREEITEM)TVI_LAST;
	tvInsertStruct.hParent = hParentItem;

	return (HTREEITEM)::SendMessage(_hSelf, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)&tvInsertStruct);
}

void TreeView::removeItem(HTREEITEM hTreeItem)
{
	// Deallocate all the sub-entries recursively
	cleanSubEntries(hTreeItem);

	// Deallocate current entry
	TVITEM tvItem;
	tvItem.hItem = hTreeItem;
	tvItem.mask = TVIF_PARAM;
	SendMessage(_hSelf, TVM_GETITEM, 0,(LPARAM)&tvItem);
	if (tvItem.lParam)
		delete (generic_string *)(tvItem.lParam);

	// Remove the node
	TreeView_DeleteItem(_hSelf, hTreeItem);
}

void TreeView::removeAllItems()
{
	for (HTREEITEM tvProj = getRoot();
        tvProj != NULL;
        tvProj = getNextSibling(tvProj))
	{
		cleanSubEntries(tvProj);
	}
	TreeView_DeleteAllItems(_hSelf);
}

void TreeView::cleanSubEntries(HTREEITEM hTreeItem)
{
	for (HTREEITEM hItem = getChildFrom(hTreeItem); hItem != NULL; hItem = getNextSibling(hItem))
	{
		TVITEM tvItem;
		tvItem.hItem = hItem;
		tvItem.mask = TVIF_PARAM;
		SendMessage(_hSelf, TVM_GETITEM, 0,(LPARAM)&tvItem);
		if (tvItem.lParam)
		{
			delete (generic_string *)(tvItem.lParam);
		}
		cleanSubEntries(hItem);
	}
}

void TreeView::setItemImage(HTREEITEM hTreeItem, int iImage, int iSelectedImage)
{
	TVITEM tvItem;
	tvItem.hItem = hTreeItem;
	tvItem.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.iImage = iImage;
	tvItem.iSelectedImage = iSelectedImage;
	TreeView_SetItem(_hSelf, &tvItem);
}

// pass LPARAM of WM_NOTIFY here after casted to NMTREEVIEW*
void TreeView::beginDrag(NMTREEVIEW* tv)
{
    // create dragging image for you using TVM_CREATEDRAGIMAGE
    // You have to delete it after drop operation, so remember it.
    _draggedItem = tv->itemNew.hItem;
    _draggedImageList = (HIMAGELIST)::SendMessage(_hSelf, TVM_CREATEDRAGIMAGE, (WPARAM)0, (LPARAM)_draggedItem);

    // start dragging operation
    // PARAMS: HIMAGELIST, imageIndex, xHotspot, yHotspot
    ::ImageList_BeginDrag(_draggedImageList, 0, 0, 0);
    ::ImageList_DragEnter(_hSelf, tv->ptDrag.x, tv->ptDrag.y);

    // redirect mouse input to the parent window
    ::SetCapture(::GetParent(_hSelf));
    ::ShowCursor(false);          // hide the cursor

    _isItemDragged = true;
}

void TreeView::dragItem(HWND parentHandle, int x, int y)
{
    // convert the dialog coords to control coords
    POINT point;
    point.x = (SHORT)x;
    point.y = (SHORT)y;
    ::ClientToScreen(parentHandle, &point);
    ::ScreenToClient(_hSelf, &point);

    // drag the item to the current the cursor position
    ::ImageList_DragMove(point.x, point.y);

    // hide the dragged image, so the background can be refreshed
    ::ImageList_DragShowNolock(false);

    // find out if the pointer is on an item
    // If so, highlight the item as a drop target.
    TVHITTESTINFO hitTestInfo;
    hitTestInfo.pt.x = point.x;
    hitTestInfo.pt.y = point.y;
    HTREEITEM targetItem = (HTREEITEM)::SendMessage(_hSelf, TVM_HITTEST, (WPARAM)0, (LPARAM)&hitTestInfo);
    if(targetItem)
    {
        // highlight the target of drag-and-drop operation
        ::SendMessage(_hSelf, TVM_SELECTITEM, (WPARAM)(TVGN_DROPHILITE), (LPARAM)targetItem);
    }

    // show the dragged image
    ::ImageList_DragShowNolock(true);

/*	
    ImageList_DragMove(x-32, y-25); // where to draw the drag from

    ImageList_DragShowNolock(FALSE);
    // the highlight items should be as

    // the same points as the drag
TVHITTESTINFO tvht;
    tvht.pt.x = x-20; 
    tvht.pt.y = y-20; //
HTREEITEM hitTarget=(HTREEITEM)SendMessage(parentHandle,TVM_HITTEST,NULL,(LPARAM)&tvht);
    if (hitTarget) // if there is a hit
		SendMessage(parentHandle,TVM_SELECTITEM,TVGN_DROPHILITE,(LPARAM)hitTarget); // highlight it

    ImageList_DragShowNolock(TRUE); 
*/
}

void TreeView::dropItem()
{
    // get the target item
    HTREEITEM targetItem = (HTREEITEM)::SendMessage(_hSelf, TVM_GETNEXTITEM, (WPARAM)TVGN_DROPHILITE, (LPARAM)0);

    // make a copy of the dragged item and insert the clone under
    // the target item, then, delete the original dragged item
    // Note that the dragged item may have children. In this case,
    // you have to move (copy and delete) for every child items, too.
    moveTreeViewItem(_draggedItem, targetItem);

    // finish drag-and-drop operation
    ::ImageList_EndDrag();
    ::ImageList_Destroy(_draggedImageList);
    ::ReleaseCapture();
    ::ShowCursor(true);
	    
	SendMessage(_hSelf,TVM_SELECTITEM,TVGN_CARET,(LPARAM)targetItem);
    SendMessage(_hSelf,TVM_SELECTITEM,TVGN_DROPHILITE,0);

    // clear global variables
    _draggedItem = 0;
    _draggedImageList = 0;
    _isItemDragged = false;
	
	/*
	ImageList_DragLeave(_hSelf);
    ImageList_EndDrag();
    HTREEITEM Selected=(HTREEITEM)SendMessage(_hSelf,TVM_GETNEXTITEM,TVGN_DROPHILITE,0);
    SendMessage(_hSelf,TVM_SELECTITEM,TVGN_CARET,(LPARAM)Selected);
    SendMessage(_hSelf,TVM_SELECTITEM,TVGN_DROPHILITE,0);
    ReleaseCapture();
    ShowCursor(TRUE); 
    _isItemDragged = FALSE;
	*/
}

void TreeView::moveTreeViewItem(HTREEITEM draggedItem, HTREEITEM targetItem)
{
	
	TCHAR textBuffer[MAX_PATH];
	TVITEM tvItem;
	tvItem.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
	tvItem.pszText = textBuffer;
	tvItem.cchTextMax = MAX_PATH;
	tvItem.hItem = draggedItem;
	SendMessage(_hSelf, TVM_GETITEM, 0,(LPARAM)&tvItem);

    TVINSERTSTRUCT tvInsertStruct;
	tvInsertStruct.item = tvItem; 
	tvInsertStruct.hInsertAfter = (HTREEITEM)TVI_LAST;
	tvInsertStruct.hParent = targetItem;

	::SendMessage(_hSelf, TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)&tvInsertStruct);
	
}