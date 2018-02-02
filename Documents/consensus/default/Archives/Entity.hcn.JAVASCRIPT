<!---- BENEFITS:
	1. no need to handle Toggle-is->action in Session.story
	2. no round-trip to server
	Issue: causes XSS Auditor error when saved from Edit.hcn
---->
<html>
<head>
<link rel="stylesheet" href="http://localhost/css/style.css" type="text/css" media="screen">
<script type="text/javascript">
function toggle() {
	var x = document.getElementById( "toggle_src" );
	var y = document.getElementById( "toggle_dst" );
	var b = document.getElementsByName( "binding" )[ 0 ];
	var value = x.innerHTML;
	if ( value == "via") {
		x.innerHTML="to";
		y.innerHTML="from";
		b.value = "ToFrom";
	}
	else if ( value == "to" ) {
		x.innerHTML="from";
		y.innerHTML="via";
		b.value = "FromVia";
	}
	else if ( value == "from" ) {
		x.innerHTML="via";
		y.innerHTML="to";
		b.value = "ViaTo";
	}
}
</script>
</head>
<body>
<div class="stamp">
	<a href="consensus">Consensus</a>
	> <a href="consensus?session=$.[ %session ]&view=Session">$.[ %session ]</a>
	> $.[ %entity ]
</div>
<div class = "page">
<h1>
Entity
</h1>
<div style="float:right;margin-right:5px;">
<a href="consensus?session=$.[ %session ]&entity=%entity&action=Remove">
	<em>Remove</em>
</a>
</div>
<form method="POST" enctype="multipart/form-data">
<input type="hidden" name="session" value="$.[ %session ]" >
<input type="hidden" name="entity" value="%entity" >
<input type="hidden" name="view" value="Entity" >
<input type="hidden" name="ext" value="hcn" >
<dl>
<dt>
Name
</dt>
<dd>
<< ?!: ...: %entity >>
	<< : source: %[ ?..: %entity ] >>
	<< : medium: %[ .?.: %entity ] >>
	<< : target: %[ ..?: %entity ] >>
	<< ?!: ...: %source >>
		<font color=LightGray> [ </font>
		<a href="consensus?session=$.[ %session ]&entity=%source">$.[ %source ]</a>
		<font color=LightGray> ] </font>
	<< /~ >>
		<a href="consensus?session=$.[ %session ]&entity=%source">$.[ %source ]</a>
	<< / >>
	<font color=LightGray> - </font>
	<< ?!: ...: %medium >>
		<font color=LightGray> [ </font>
		<a href="consensus?session=$.[ %session ]&entity=%medium">$.[ %medium ]</a>
		<font color=LightGray> ] </font>
	<< /~ >>
		<a href="consensus?session=$.[ %session ]&entity=%medium">$.[ %medium ]</a>
	<< / >>
	<font color=LightGray> -> </font>
	<< ?!: ...: %target >>
		<font color=LightGray> [ </font>
		<a href="consensus?session=$.[ %session ]&entity=%target">$.[ %target ]</a>
		<font color=LightGray> ] </font>
	<< /~ >>
		<a href="consensus?session=$.[ %session ]&entity=%target">$.[ %target ]</a>
	<< / >>
<< /~ >>
	$.[ %entity ]
<< / >>
</dd>

<< ?!: !!!: %entity >>
<dt>
Instances
</dt>
<dd>
	<table style="width:50vw">
	<< ?: !..: %entity >>
		<tr>
		<td>
		<< : medium : %[ .?. : %? ] >>
		<< : target : %[ ..? : %? ] >>
		<< ?!: ...: %entity >>
			<font color=LightGray> [ </font>
			$.[ %entity ]
			<font color=LightGray> ] </font>
		<< /~ >>
			$.[ %entity ]
		<< / >>
		<font color=LightGray> - </font>
		<< ?!: ...: %medium >>
			<font color=LightGray> [ </font>
			<a href="consensus?session=$.[ %session ]&entity=%medium">$.[ %medium ]</a>
			<font color=LightGray> ] </font>
		<< /~ >>
			<a href="consensus?session=$.[ %session ]&entity=%medium">$.[ %medium ]</a>
		<< / >>
		<font color=LightGray> -> </font>
		<< ?!: ...: %target >>
			<font color=LightGray> [ </font>
			<a href="consensus?session=$.[ %session ]&entity=%target">$.[ %target ]</a>
			<font color=LightGray> ] </font>
		<< /~ >>
			<a href="consensus?session=$.[ %session ]&entity=%target">$.[ %target ]</a>
		<< / >>
		</td>
		<td style="width:8%;text-align:center">
			<span style="color:LightGray">[ </span>
			<a style="font-size:70%" href="consensus?session=$.[ %session ]&entity=%?"> >> </a>
			<span style="color:LightGray"> ]</span>
		</td>
		</tr>
	<< / >>
	<< ?: .!.: %entity >>
		<tr>
		<td>
		<< : source : %[ ?.. : %? ] >>
		<< : target : %[ ..? : %? ] >>
		<< ?!: ...: %source >>
			<font color=LightGray> [ </font>
			<a href="consensus?session=$.[ %session ]&entity=%source">$.[ %source ]</a>
			<font color=LightGray> ] </font>
		<< /~ >>
			<a href="consensus?session=$.[ %session ]&entity=%source">$.[ %source ]</a>
		<< / >>
		<font color=LightGray> - </font>
		<< ?!: ...: %entity >>
			<font color=LightGray> [ </font>
			$.[ %entity ]
			<font color=LightGray> ] </font>
		<< /~ >>
			$.[ %entity ]
		<< / >>
		<font color=LightGray> -> </font>
		<< ?!: ...: %target >>
			<font color=LightGray> [ </font>
			<a href="consensus?session=$.[ %session ]&entity=%target">$.[ %target ]</a>
			<font color=LightGray> ] </font>
		<< /~ >>
			<a href="consensus?session=$.[ %session ]&entity=%target">$.[ %target ]</a>
		<< / >>
		</td>
		<td style="width:8%;text-align:center">
			<span style="color:LightGray">[ </span>
			<a style="font-size:70%" href="consensus?session=$.[ %session ]&entity=%?"> >> </a>
			<span style="color:LightGray"> ]</span>
		</td>
		</tr>
	<< / >>
	<< ?: ..!: %entity >>
		<tr>
		<td>
		<< : source : %[ ?.. : %? ] >>
		<< : medium : %[ .?. : %? ] >>
		<< ?!: ...: %source >>
			<font color=LightGray> [ </font>
			<a href="consensus?session=$.[ %session ]&entity=%source">$.[ %source ]</a>
			<font color=LightGray> ] </font>
		<< /~ >>
			<a href="consensus?session=$.[ %session ]&entity=%source">$.[ %source ]</a>
		<< / >>
		<font color=LightGray> - </font>
		<< ?!: ...: %medium >>
			<font color=LightGray> [ </font>
			<a href="consensus?session=$.[ %session ]&entity=%medium">$.[ %medium ]</a>
			<font color=LightGray> ] </font>
		<< /~ >>
			<a href="consensus?session=$.[ %session ]&entity=%medium">$.[ %medium ]</a>
		<< / >>
		<font color=LightGray> -> </font>
		<< ?!: ...: %entity >>
			<font color=LightGray> [ </font>
			$.[ %entity ]
			<font color=LightGray> ] </font>
		<< /~ >>
			$.[ %entity ]
		<< / >>
		</td>
		<td style="width:8%;text-align:center">
			<span style="color:LightGray">[ </span>
			<a style="font-size:70%" href="consensus?session=$.[ %session ]&entity=%?"> >> </a>
			<span style="color:LightGray"> ]</span>
		</td>
		</tr>
	<< / >>
	</table>
	<p>
</dd>
<< / >>

<dt>
Connect
</dt>
<dd>
<input type="hidden" name="binding" value="ViaTo">
<table style="width:60vw">
<tr>
<td style="color:BlueViolet; font-style:italic; cursor:pointer;">
	<span id="toggle_src" onclick="toggle()">via</span>
</td>
<td style="padding-right:10px;">
	<input type="text" style="font-size:90%; padding:3px; width:100%;" name="arg1" autocomplete="off">
</td>
<td style="color:Gray; font-style:italic;">
	<span id="toggle_dst">to</span>
</td>
<td style="padding-right:10px;">
	<input type="text" style="font-size:90%; padding:3px; width:100%;" name="arg2" autocomplete="off">
</td>
<td style="width:6%;text-align:center;">
	<input type="submit" class="button" style="font-size:80%;font-style:italic;" name="action" value="Link">
</td>
</tr>
</table>
</dd>

<dt>
Description
</dt>
<dd>
<em>no description</em>
</dd>

</dl>
<input type="submit" class="button" style="float:right;" name="action" value="Edit">
</form>
</div class="page">
<div class="footer">
<hr>
<div class="date">
September 19, 2017
</div>
</div>
</body>
</html>
