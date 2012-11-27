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

	foreach (glob("*/m_*.cpp") as $sFile)
	{
		$aOut = array();
		exec("git ls-files --error-unmatch -- " . $sFile . " 2>/dev/null", $aOut, $exitStatus);
		
		if ($exitStatus > 0)
		{
			echo "Skipping " . $sFile . " which is not in git\n";
			continue;
		}
		
		$aTags = get_tags($sFile);

		$niceVer = array();
		exec("git rev-list HEAD -- " . $sFile . " 2>/dev/null", $niceVer);
		$verFullNum = preg_replace('/(.*)\/m_.*/', "$1", $sFile) . "." . (count($niceVer) - 1);

		$sStrippedName = preg_replace('/.*m_(.*).cpp/', "m_$1", $sFile);
		$sText .= "module " . $sStrippedName . " " . $verFullNum . " https://raw.github.com/inspircd/inspircd-extras/" . $niceVer[0] . "/" . $sFile . "\n";

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
