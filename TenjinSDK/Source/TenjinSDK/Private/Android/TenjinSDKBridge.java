// Copyright (c) Tenjin. All Rights Reserved.
//
// Java helper invoked by Unreal via JNI. Owns a single TenjinSDK instance and
// forwards calls. All methods are static; the JNI signatures are kept stable
// and match TenjinBPLibrary.cpp's expectations.
//
// Installed into the packaged APK via TenjinSDK_UPL_Android.xml.

package com.tenjin.unreal;

import android.app.Activity;
import android.util.Log;

import com.tenjin.android.AttributionInfoCallback;
import com.tenjin.android.Callback;
import com.tenjin.android.TenjinSDK;
import com.tenjin.android.TenjinSDK.AppStoreType;

import org.json.JSONObject;

import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;

public final class TenjinSDKBridge {
	private static final String TAG = "TenjinSDKBridge";
	private static TenjinSDK sInstance;
	private static final AtomicReference<String> sCachedAttribution = new AtomicReference<>("");

	private TenjinSDKBridge() {}

	public static synchronized void initialize(Activity activity, String sdkKey) {
		if (activity == null) {
			Log.e(TAG, "initialize called with null activity");
			return;
		}
		sInstance = TenjinSDK.getInstance(activity, sdkKey);
	}

	public static void connect() {
		if (sInstance != null) sInstance.connect();
	}

	public static void connectWithDeferredDeeplink(String deferredDeeplink) {
		if (sInstance != null) sInstance.connect(deferredDeeplink);
	}

	public static void setAppStore(String type) {
		if (sInstance == null) return;
		AppStoreType storeType;
		switch (type) {
			case "googleplay": storeType = AppStoreType.googleplay; break;
			case "amazon":     storeType = AppStoreType.amazon; break;
			case "other":      storeType = AppStoreType.other; break;
			default:           storeType = AppStoreType.unspecified; break;
		}
		sInstance.setAppStore(storeType);
	}

	public static void optIn()                 { if (sInstance != null) sInstance.optIn(); }
	public static void optOut()                { if (sInstance != null) sInstance.optOut(); }
	public static void optInOutUsingCMP()      { if (sInstance != null) sInstance.optInOutUsingCMP(); }
	public static void optInGoogleDMA()        { if (sInstance != null) sInstance.optInGoogleDMA(); }
	public static void optOutGoogleDMA()       { if (sInstance != null) sInstance.optOutGoogleDMA(); }

	public static void optInParams(String[] params) {
		if (sInstance != null) sInstance.optInParams(params);
	}

	public static void optOutParams(String[] params) {
		if (sInstance != null) sInstance.optOutParams(params);
	}

	public static void setGoogleDMAParameters(boolean adPersonalization, boolean adUserData) {
		if (sInstance != null) sInstance.setGoogleDMAParameters(adPersonalization, adUserData);
	}

	public static void setCacheEventSetting(boolean setting) {
		if (sInstance != null) sInstance.setCacheEventSetting(setting);
	}

	public static void setEncryptRequestsSetting(boolean setting) {
		if (sInstance != null) sInstance.setEncryptRequestsSetting(setting);
	}

	public static void appendAppSubversion(int version) {
		if (sInstance != null) sInstance.appendAppSubversion(version);
	}

	public static void eventWithName(String name) {
		if (sInstance != null) sInstance.eventWithName(name);
	}

	public static void eventWithNameAndValue(String name, int value) {
		if (sInstance != null) sInstance.eventWithNameAndValue(name, value);
	}

	public static void transaction(String productName, String currencyCode,
	                               int quantity, double unitPrice) {
		if (sInstance != null) sInstance.transaction(productName, currencyCode, quantity, unitPrice);
	}

	public static void transactionWithDataSignature(String productName, String currencyCode,
	                                                int quantity, double unitPrice,
	                                                String purchaseData, String dataSignature) {
		if (sInstance != null) {
			sInstance.transaction(productName, currencyCode, quantity, unitPrice,
			                      purchaseData, dataSignature);
		}
	}

	public static void setCustomerUserId(String userId) {
		if (sInstance != null) sInstance.setCustomerUserId(userId);
	}

	public static String getCustomerUserId() {
		return sInstance != null && sInstance.getCustomerUserId() != null
				? sInstance.getCustomerUserId() : "";
	}

	public static String getAnalyticsInstallationId() {
		return sInstance != null && sInstance.getAnalyticsInstallationId() != null
				? sInstance.getAnalyticsInstallationId() : "";
	}

	/**
	 * Synchronously returns the last cached attribution JSON. Attribution is
	 * fetched asynchronously by the native SDK; in practice callers fire this
	 * after Connect() and re-poll if empty. Returns "" when nothing has arrived.
	 */
	public static String getAttributionInfoJson() {
		if (sInstance == null) return "";
		// Best-effort: kick off a fetch so subsequent calls have data.
		sInstance.getAttributionInfo(new AttributionInfoCallback() {
			@Override
			public void onSuccess(Map<String, String> data) {
				sCachedAttribution.set(data != null ? new JSONObject(data).toString() : "{}");
			}
		});
		return sCachedAttribution.get();
	}

	public static String getUserProfileJson() {
		if (sInstance == null) return "{}";
		try {
			Map<String, Object> profile = sInstance.getUserProfileDictionary();
			return profile != null ? new JSONObject(profile).toString() : "{}";
		} catch (Throwable t) {
			return "{}";
		}
	}

	public static void resetUserProfile() {
		if (sInstance != null) sInstance.resetUserProfile();
	}

	/**
	 * Fetches the deep link for this session. The Tenjin Android SDK's deeplink
	 * API is one-shot (unlike iOS's persistent registerDeepLinkHandler:); to
	 * preserve cross-platform API parity, calling RegisterDeepLinkHandler on
	 * Unreal triggers a single fetch and forwards the result through C++.
	 *
	 * Callback payload (boolean clickedTenjinLink, boolean isFirstSession,
	 * Map<String, String> params) is serialized into JSON with two synthetic
	 * boolean keys joining the params map.
	 */
	public static synchronized void registerDeepLinkHandler() {
		if (sInstance == null) return;
		sInstance.getDeeplink(new Callback() {
			@Override
			public void onSuccess(boolean clickedTenjinLink, boolean isFirstSession,
			                      Map<String, String> params) {
				try {
					JSONObject payload = params != null ? new JSONObject(params) : new JSONObject();
					payload.put("clicked_tenjin_link", clickedTenjinLink);
					payload.put("is_first_session", isFirstSession);
					nativeOnDeepLink(payload.toString());
				} catch (Throwable t) {
					Log.w(TAG, "getDeeplink: " + t.getMessage());
				}
			}
		});
	}

	private static native void nativeOnDeepLink(String json);

	// ---- ILRD --------------------------------------------------------------

	public static void eventAdImpressionAdMob(String json)      { sendImpression("adMob", json); }
	public static void eventAdImpressionAppLovin(String json)   { sendImpression("appLovin", json); }
	public static void eventAdImpressionHyperBid(String json)   { sendImpression("hyperBid", json); }
	public static void eventAdImpressionIronSource(String json) { sendImpression("ironSource", json); }
	public static void eventAdImpressionTopOn(String json)      { sendImpression("topOn", json); }
	public static void eventAdImpressionTradPlus(String json)   { sendImpression("tradPlus", json); }

	private static void sendImpression(String network, String json) {
		if (sInstance == null) return;
		try {
			JSONObject obj = new JSONObject(json);
			switch (network) {
				case "adMob":      sInstance.eventAdImpressionAdMob(obj); break;
				case "appLovin":   sInstance.eventAdImpressionAppLovin(obj); break;
				case "hyperBid":   sInstance.eventAdImpressionHyperBid(obj); break;
				case "ironSource": sInstance.eventAdImpressionIronSource(obj); break;
				case "topOn":      sInstance.eventAdImpressionTopOn(obj); break;
				case "tradPlus":   sInstance.eventAdImpressionTradPlus(obj); break;
			}
		} catch (Throwable t) {
			Log.w(TAG, "Failed to send " + network + " impression: " + t.getMessage());
		}
	}
}
