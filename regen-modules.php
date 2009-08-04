<?php

function get_tags($sFile)
{
	$mfc = file_get_contents($sFile);
    preg_match_all('@/\*\s*\$([^ ]+)\:\s*(.+?)\s*\*/@i', $mfc, $matches, PREG_SET_ORDER);


	foreach ($matches as $amatch)
	{
		$lolmatch[$amatch[1]][] = $amatch[2];
	}

	return $lolmatch;
}


	$sText = "";

	foreach (glob("m_*.cpp") as $sFile)
	{
		$aOut = array();
		exec("git log -n 1 " . $sFile, $aOut);
		$aOut = explode(" ", $aOut[0]);

		$aTags = get_tags($sFile);

		if (empty($aOut[1]))
		{
			echo "Skipping " . $sStrippedName . " which is not in git\n";
			continue;
		}
		

		$sStrippedName = str_replace(".cpp", "", $sFile);
		$sText .= "module " . $sStrippedName . " " . $aOut[1] . " http://gitorious.org/inspircd/inspircd-extras/blobs/raw/master/" . $sFile . "\n";

		if ($aTags['ModDepends'])
		{
			foreach ($aTags['ModDepends'] as $sDepend)
				$sText .= " depends " . $sDepend . "\n";
		}

		if ($aTags['ModConflicts'])
		{
			foreach ($aTags['ModConflicts'] as $sConflicts)
				$sText .= " conflicts " . $sConflicts . "\n";
		}

		if ($aTags['ModDesc'])
		{
			foreach ($aTags['ModDesc'] as $sDesc)
				$sText .= " description " . $sDesc . "\n";
		}

		if ($aTags['ModMask'])
		{
			foreach ($aTags['ModMask'] as $sMask)
				$sText .= " mask " . $sMask . "\n";
		}
	}

	file_put_contents("modules.lst", $sText);
?>
