package com.none.xylremote;

import java.util.LinkedList;
import java.util.List;

import android.annotation.SuppressLint;
import android.content.Context;
import android.hardware.ConsumerIrManager;
import android.util.Log;

import com.lge.hardware.IRBlaster.IRBlaster;
import com.lge.hardware.IRBlaster.IRBlasterCallback;
import com.lge.hardware.IRBlaster.ResultCode;

@SuppressLint("NewApi")
public class IRControl {

	@SuppressLint("InlinedApi")
	public IRControl(Context cxt) {
        mCIR = (ConsumerIrManager)cxt.getSystemService(Context.CONSUMER_IR_SERVICE);
		mLGIR = null;
		if (IRBlaster.isSdkSupported(cxt)) {
			mLGIR = IRBlaster.getIRBlaster(cxt, mIrBlasterReadyCallback);
		}
		if (mLGIR == null) {
			Log.e("MyActivity", "No IR Blaster in this device");
			return;
		}	
	}
	
	public boolean transmit(int frequency, int[] pattern) {

		if (mCIR.hasIrEmitter()) {
		    mCIR.transmit(frequency, pattern);
			return true;
	    }

		if (mLGIR != null) {
			Log.e("MainActivity", "No LG IR Emitter found\n");
			mLGIR.sendIRPattern(frequency, pattern);
			return true;
		}
		return false;
	}

	public void encodeNECValue(List seq, long value) {
		seq.add(NEC_HDR_MARK); 
		seq.add(NEC_HDR_SPACE);
		for (int bitidx=31; bitidx >=0; --bitidx) {
			seq.add(NEC_BIT_MARK);
			int space = ((value & ((long)(1) << bitidx )) != 0) ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
			seq.add(space);
		}
		seq.add(NEC_HDR_MARK);
	}
	
	public void encodeNECRepeat(List seq) {
		seq.add(NEC_HDR_MARK);
		seq.add(NEC_RPT_SPACE);
		seq.add(NEC_BIT_MARK);
	}

	public boolean transmitNECValue(long value, int repeat) {
		Log.i("IRControl", "transmiting value: " + value + " ; with repeat: " + repeat);
		List<Integer> seq = new LinkedList<Integer>();
		encodeNECValue(seq, value);		
		for (int idx=0; idx < repeat; ++idx) {
			seq.add(NEC_STD_DELAY);
			encodeNECRepeat(seq);
		}
		
		int[] pattern = new int[seq.size()];
		for(int i = 0; i < seq.size(); i++) pattern[i] = seq.get(i);
		return transmit(FREQUENCY, pattern);
		
	}

	 
	public IRBlasterCallback mIrBlasterReadyCallback = new IRBlasterCallback() {

		@Override
		public void IRBlasterReady() {
			Log.i("IRControl", "IRBlaster is really ready");

		}

		@Override
		public void learnIRCompleted(int status) {
			Log.i("IRControl", "Learn IR complete");
		}

		@Override
		public void newDeviceId(int id) {
			Log.i("IRControl", "Added Device Id: " + id);
		}

		@Override
		public void failure(int error) {
			Log.e("IRControl", "Error: " + ResultCode.getString(error));
		}
	};
	
	private IRBlaster mLGIR;
	private ConsumerIrManager mCIR;
	static final int FREQUENCY = 38000;
	static final int NEC_HDR_MARK = 9000;
	static final int NEC_HDR_SPACE = 4500;
	static final int NEC_BIT_MARK = 560;
	static final int NEC_RPT_SPACE	= 2250;
	static final int NEC_ONE_LEN = 2250;
	static final int NEC_ZERO_LEN = 1125;
	static final int NEC_ONE_SPACE = NEC_ONE_LEN - NEC_BIT_MARK; //1690;
	static final int NEC_ZERO_SPACE	= NEC_ZERO_LEN - NEC_BIT_MARK; //560;
	static final int NEC_STD_DELAY = 110000;
	

 
;
	
}
