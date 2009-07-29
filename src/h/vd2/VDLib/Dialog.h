#ifndef f_VD2_VDLIB_DIALOG_H
#define f_VD2_VDLIB_DIALOG_H

#ifdef _MSC_VER
#pragma once
#endif

#include <vd2/system/vdstl.h>
#include <vd2/system/win32/miniwindows.h>
#include <vd2/VDLib/UIProxies.h>

class IVDUIDropFileList {
public:
	virtual bool GetFileName(int index, VDStringW& fileName) = 0;
};

class VDDialogFrameW32 {
public:
	VDZHWND GetWindowHandle() const { return mhdlg; }

	bool	Create(VDGUIHandle hwndParent);
	void	Destroy();

	void	Show();
	void	Hide();

	sintptr ShowDialog(VDGUIHandle hwndParent);

protected:
	VDDialogFrameW32(uint32 dlgid);

	void End(sintptr result);

	void AddProxy(VDUIProxyControl *proxy, uint32 id);

	VDZHWND GetControl(uint32 id);

	void SetFocusToControl(uint32 id);
	void EnableControl(uint32 id, bool enabled);

	void SetCaption(uint32 id, const wchar_t *format);

	void SetControlText(uint32 id, const wchar_t *s);
	void SetControlTextF(uint32 id, const wchar_t *format, ...);

	uint32 GetControlValueUint32(uint32 id);
	double GetControlValueDouble(uint32 id);
	VDStringW GetControlValueString(uint32 id);

	void ExchangeControlValueBoolCheckbox(bool write, uint32 id, bool& val);
	void ExchangeControlValueDouble(bool write, uint32 id, const wchar_t *format, double& val, double minVal, double maxVal);
	void ExchangeControlValueString(bool write, uint32 id, VDStringW& s);

	void CheckButton(uint32 id, bool checked);
	bool IsButtonChecked(uint32 id);

	void BeginValidation();
	bool EndValidation();

	void FailValidation(uint32 id);
	void SignalFailedValidation(uint32 id);

	// listbox
	void LBClear(uint32 id);
	sint32 LBGetSelectedIndex(uint32 id);
	void LBSetSelectedIndex(uint32 id, sint32 idx);
	void LBAddString(uint32 id, const wchar_t *s);
	void LBAddStringF(uint32 id, const wchar_t *format, ...);

	// trackbar
	sint32 TBGetValue(uint32 id);
	void TBSetValue(uint32 id, sint32 value);
	void TBSetRange(uint32 id, sint32 minval, sint32 maxval);

protected:
	virtual VDZINT_PTR DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);
	virtual void OnDataExchange(bool write);
	virtual bool OnLoaded();
	virtual bool OnOK();
	virtual bool OnCancel();
	virtual void OnSize();
	virtual void OnDestroy();
	virtual bool OnTimer(uint32 id);
	virtual bool OnCommand(uint32 id, uint32 extcode);
	virtual void OnDropFiles(VDZHDROP hDrop);
	virtual void OnDropFiles(IVDUIDropFileList *dropFileList);
	virtual void OnHScroll(uint32 id, int code);
	virtual void OnVScroll(uint32 id, int code);
	virtual bool PreNCDestroy();

	bool	mbValidationFailed;
	bool	mbIsModal;
	VDZHWND	mhdlg;

private:
	static VDZINT_PTR VDZCALLBACK StaticDlgProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam);

	const char *mpDialogResourceName;
	uint32	mFailedId;

protected:
	VDUIProxyMessageDispatcherW32 mMsgDispatcher;
};

class VDDialogResizerW32 {
public:
	VDDialogResizerW32();
	~VDDialogResizerW32();

	enum {
		kAnchorX1_C	= 0x01,
		kAnchorX1_R	= 0x02,
		kAnchorX2_C	= 0x04,
		kAnchorX2_R	= 0x08,
		kAnchorY1_M	= 0x10,
		kAnchorY1_B	= 0x20,
		kAnchorY2_M	= 0x40,
		kAnchorY2_B	= 0x80,

		kL		= 0,
		kC		= kAnchorX2_R,
		kR		= kAnchorX2_R | kAnchorX1_R,
		kHMask	= 0x0F,

		kT		= 0,
		kM		= kAnchorY2_B,
		kB		= kAnchorY2_B | kAnchorY1_B,
		kVMask	= 0xF0,

		kX1Y1Mask = 0x33,
		kX2Y2Mask = 0xCC,

		kTL		= kT | kL,
		kTR		= kT | kR,
		kTC		= kT | kC,
		kML		= kM | kL,
		kMR		= kM | kR,
		kMC		= kM | kC,
		kBL		= kB | kL,
		kBR		= kB | kR,
		kBC		= kB | kC,
	};

	void Init(VDZHWND hwnd);
	void Relayout();
	void Relayout(int width, int height);
	void Add(uint32 id, int alignment);

protected:
	struct ControlEntry {
		VDZHWND	mhwnd;
		int		mAlignment;
		sint32	mX1;
		sint32	mY1;
		sint32	mX2;
		sint32	mY2;
	};

	VDZHWND	mhwndBase;
	int		mWidth;
	int		mHeight;

	typedef vdfastvector<ControlEntry> Controls;
	Controls mControls;
};

#endif
