var toggleInit = false;
var current_toggle_pos = false;
var verification_url_update_interval_period_ms = 10000;
var verification_toggle_update_interval_period_ms = 10000;

function update_verification_url_display() {
	$('#setVerificationServerUrlResponse').parent().hide();
	$.ajax({
		type: 'GET',
		url: '/monitor/verification-url',
		crossDomain: true,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			var ret = responseData.replace("verification-url: ", '');
			$('#verificationUrlInUse').text(ret);
		},
		error: function (responseData, textStatus, errorThrown) {
			$('#verificationUrlInUse').text("Unknown");
		}
	});
}

function update_verification_toggle() {
	$('#verificationSettingResponse').parent().hide();
	$.ajax({
		type: 'GET',
		url: '/monitor/verification-option',
		crossDomain: true,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			try {
				// Mark toggle initialized
				toggleInit = true;
				if (responseData === "verification-enabled: true") {
					current_toggle_pos = true;
					$('#verificationToggle').prop('checked', true);
				} else if (responseData === "verification-enabled: false") {
					current_toggle_pos = false;
					$('#verificationToggle').prop('checked', false);
				}
			} catch (error) {
				console.log("Failed to update Verification Toggle. Error Msg: " + error);
			}
		},
		error: function (responseData, textStatus, errorThrown) {
			// Mark as initialized on error as well, as else nothing about the toggle will work
			toggleInit = true;
			console.log("Update Verification Toggle error case, server response: " + responseData);
		}
	});
}

function register_set_verification_server_url_button_onclick() {
	// Script for handling Verification Server URL setting and getting
	$('#setVerificationServerUrlButton').click(function () {
		$('#setVerificationServerUrlResponse').text('Waiting');
		var verif_url = $('#verificationServerUrlInput').val();
		$.ajax({
			type: 'POST',
			url: '/admin/set-verification-url',
			crossDomain: true,
			data: { 'verification-url': verif_url },
			dataType: 'text',
			success: function (responseData, textStatus, jqXHR) {
				info_str = "Set verification server url to " + verif_url + ".";
				console.log(info_str);
				$('#setVerificationServerUrlResponse').parent().show();
				$('#setVerificationServerUrlResponse').text(info_str);
				$('#setVerificationServerUrlResponse').parent().removeClass('alert-danger');
				$('#setVerificationServerUrlResponse').parent().addClass('alert-success');
			},
			error: function (responseData, textStatus, errorThrown) {
				info_str = "Failed to set verification server url to " + verif_url + ".";
				console.log(info_str);
				$('#setVerificationServerUrlResponse').parent().show();
				$('#setVerificationServerUrlResponse').text(info_str);
				$('#setVerificationServerUrlResponse').parent().removeClass('alert-success');
				$('#setVerificationServerUrlResponse').parent().addClass('alert-danger');
			}
		});
	});
}

function register_verification_toggle_onchange() {
	// Something (User or script) changes toggle
	$('#verificationToggle').change(function () {
		if (!toggleInit) { 
			//if not yet initialized, skip
			return;
		}
		var toggleState = $('#verificationToggle').prop('checked');
		// Check that the toggle state is different than the current aspect because this also
		// triggers when the toggle is adjusted in update_verification_toggle, in which case the
		// toggle is already in the correct aspect, thus no request has to be sent to the server
		if (toggleState == current_toggle_pos) {
			return;
		}
		// Send request to server to set verification accordingly.
		$.ajax({
			type: 'POST',
			url: '/admin/set-verification-option',
			crossDomain: true,
			data: { 'verification-option': toggleState },
			dataType: 'text',
			success: function (responseData, textStatus, jqXHR) {
				info_str = (toggleState ? "Enabled" : "Disabled") + " verification.";
				console.log(info_str);
				$('#verificationSettingResponse').parent().show();
				$('#verificationSettingResponse').text(info_str);
				$('#verificationSettingResponse').parent().removeClass('alert-danger');
				$('#verificationSettingResponse').parent().addClass('alert-success');
				current_toggle_pos = toggleState;
			},
			error: function (responseData, textStatus, errorThrown) {
				info_str = "Failed to " + (toggleState ? "enable" : "disable") + " verification.";
				console.log(info_str);
				$('#verificationSettingResponse').parent().show();
				$('#verificationSettingResponse').text(info_str);
				$('#verificationSettingResponse').parent().removeClass('alert-success');
				$('#verificationSettingResponse').parent().addClass('alert-danger');
				$('#verificationToggle').prop('checked', current_toggle_pos);
			}
		});
	});
}

$(document).ready(
	function () {
		// Register the 'click' callback for the setVerificationServerUrl-Button.
		register_set_verification_server_url_button_onclick();
		
		// Update Verification URL display once and then activate the interval to update
		// it with a period of verification_url_update_interval_period_ms.
		update_verification_url_display();
		verif_url_update_interval = setInterval(() => { 
			update_verification_url_display();
		}, verification_url_update_interval_period_ms);
		
		// Update Verification Toggle once and then activate the interval to update
		// it with a period of verification_toggle_update_interval_period_ms.
		update_verification_toggle();
		verif_toggle_update_interval = setInterval(() => { 
			update_verification_toggle();
		}, verification_toggle_update_interval_period_ms);
		
		// Register the 'change' callback for the verification toggle. This comes after
		// the first update of the verification toggle intentionally, as the first update
		// initialises it, and only then watching for a 'change' makes sense.
		register_verification_toggle_onchange();
	}
);
